// Developer › ADV Controller › Decoder · ADV7280 / Encoder · ADV7391: the known options of the ADV Manager maps as rows
// grouped by register, maps as headers. Values are read on open (one map after the other; a map that does not answer
// shows its error, the others still read); changes wait in a bar and Apply sends them through the planner, then reads
// the maps again. An ADV Controller without register reads shows the update hint.
import { signal, computed, type Signal } from "@preact/signals";
import { useEffect, useRef } from "preact/hooks";
import { Icon } from "../../../art/icon.tsx";
import { device } from "../../../store/app.ts";
import { confirm } from "../../../shell/Confirm.tsx";
import { loadRegmap, advKey, type Regmap, type AdvMap, type AdvReg, type AdvField } from "../../../regs/regmap.ts";
import { planAdvWrites, planAdvReads, type AdvChange } from "../../../regs/planner.ts";
import { advRead, advWrite, advBusyUpdating, advCanRead, logLine, hx, AdvReadError } from "./adv-io.ts";
import { ConsoleDrawer } from "./ConsoleDrawer.tsx";
import { recording, recordApplied } from "./recorder.ts";

export const DANGER_TEXT = "the picture stays wrong until the input changes or the device restarts";
const MAP_NAMES: Record<string, string> = { user_sub_map: "Main", user_sub_map_2: "ACE", vpp_map: "Deinterlacer (VPP)", main: "Main" };
const OPTION_TYPES = ["e", "b", "i"];

const regmap = signal<Regmap | null | undefined>(undefined);
export const advValues = signal<Map<string, number>>(new Map());      // advKey -> byte read from the chip
export const advPending = signal<Map<string, number>>(new Map());     // advKey -> byte to write
const mapState = signal<Map<string, string>>(new Map());              // "42:00" -> "" | "reading" | error text
const notice = signal<{ tone: "good" | "bad"; text: string } | null>(null);

export const regmapNow = (): Regmap | null | undefined => regmap.value;

export function useRegmap(): Regmap | null | undefined {
  useEffect(() => { if (regmap.value === undefined) void loadRegmap((p) => device.client.get(p)).then((m) => { regmap.value = m; }); }, []);
  return regmap.value;
}

const mapKey = (m: AdvMap) => `${hx(m.i2c).toLowerCase()}:${hx(m.map).toLowerCase()}`;
const width = (f: AdvField) => f.msb - f.lsb + 1;
const fGet = (v: number, f: AdvField) => (v >> f.lsb) & ((1 << width(f)) - 1);
const fSet = (v: number, f: AdvField, x: number) => { const m = ((1 << width(f)) - 1) << f.lsb; return (v & ~m) | ((x << f.lsb) & m); };
const clean = (n: string) => n.replace(/\[.*\]$/, "");
const optionRegs = (m: AdvMap) => m.regs.filter((r) => !r.readonly && r.danger !== "planner" && r.fields.some((f) => OPTION_TYPES.includes(f.type)));

function setMap(k: string, v: string): void { const n = new Map(mapState.value); n.set(k, v); mapState.value = n; }

export async function readMaps(maps: AdvMap[]): Promise<boolean> {
  let all = true;
  for (const m of maps) {
    const k = mapKey(m);
    setMap(k, "reading");
    try {
      for (const q of planAdvReads(optionRegs(m).map((r) => ({ i2c: m.i2c, map: m.map, reg: r.addr })))) {
        const values = await advRead(q);
        const next = new Map(advValues.value);
        values.forEach((v, i) => next.set(advKey(q.dev, q.map, q.reg + i), v));
        advValues.value = next;
      }
      setMap(k, "");
      logLine("ok", `read ${MAP_NAMES[m.id] ?? m.id} (${hx(m.i2c)})`);
    } catch (e) {
      const why = e instanceof AdvReadError ? e.state : String(e);
      setMap(k, why === "error" || why === "timeout" ? "No answer" : why === "unsupported" ? "Reads need a newer ADV Controller" : `Not read (${why})`);
      logLine("err", `read ${MAP_NAMES[m.id] ?? m.id}: ${why}`);
      all = false;
    }
  }
  return all;
}

// Exactly these registers read back (presets and macros also write registers that have no option row)
export async function readRegs(keys: string[]): Promise<boolean> {
  const regs = keys.map((k) => { const [i2c, map, reg] = k.split(":").map((x) => parseInt(x, 16)); return { i2c, map, reg }; });
  try {
    for (const q of planAdvReads(regs)) {
      const values = await advRead(q);
      const next = new Map(advValues.value);
      values.forEach((v, i) => next.set(advKey(q.dev, q.map, q.reg + i), v));
      advValues.value = next;
    }
    return true;
  } catch (e) {
    logLine("err", `read back: ${e instanceof AdvReadError ? e.state : String(e)}`);
    return false;
  }
}

const byteOf = (k: string): number | undefined => advPending.value.get(k) ?? advValues.value.get(k);

function setField(m: AdvMap, r: AdvReg, f: AdvField, x: number): void {
  const k = advKey(m.i2c, m.map, r.addr);
  const cur = byteOf(k);
  if (cur === undefined) return;
  const nv = fSet(cur, f, x);
  const next = new Map(advPending.value);
  if (nv === advValues.value.get(k)) next.delete(k); else next.set(k, nv);
  advPending.value = next;
}

// Apply: the planner's requests, dangerous ones after a warning, then the touched maps read again; the change counts
// as applied only when the read shows it
export async function applyPending(rm: Regmap, maps: AdvMap[]): Promise<void> {
  const changes: AdvChange[] = [...advPending.value].map(([k, value]) => { const [i2c, map, reg] = k.split(":").map((s) => parseInt(s, 16)); return { i2c, map, reg, value }; });
  const plan = planAdvWrites(changes, rm);
  for (const s of plan.skipped) logLine("sys", `skipped ${hx(s.reg.i2c)} ${hx(s.reg.reg)}: ${s.reason}`);
  if (plan.dangerous.length) {
    const what = plan.dangerous.map((d) => `0x${hx(d.reg)}`).join(", ");
    if (!(await confirm({ icon: "warning", title: `Write ${what}?`, body: `This register can reset or power down the chip: ${DANGER_TEXT}.`, ok: "Write anyway" }))) return;
  }
  notice.value = null;
  for (const r of plan.requests) if (!(await advWrite(r))) { notice.value = { tone: "bad", text: "Not sent: the device refused the write." }; return; }
  const skipped = new Set(plan.skipped.map((x) => advKey(x.reg.i2c, x.reg.map, x.reg.reg)));
  recordApplied(changes.filter((c) => !skipped.has(advKey(c.i2c, c.map, c.reg))));   // a macro being recorded takes them
  const sent = new Map(advPending.value);
  advPending.value = new Map();
  const touched = maps.filter((m) => changes.some((c) => c.i2c === m.i2c && c.map === m.map));
  const ok = await readMaps(touched);
  const confirmed = ok && [...sent].every(([k, v]) => advValues.value.get(k) === v);
  notice.value = confirmed ? { tone: "good", text: `Applied ${sent.size} register${sent.size === 1 ? "" : "s"}.` }
    : { tone: "bad", text: ok ? "Sent, but the chip shows other values: the ADV Controller may have set them again." : "Sent, but not confirmed: the ADV Controller did not answer the read." };
  logLine(confirmed ? "ok" : "err", notice.value.text);
}

function FieldRow({ m, r, f }: { m: AdvMap; r: AdvReg; f: AdvField }) {
  const k = advKey(m.i2c, m.map, r.addr);
  const v = byteOf(k);
  const x = v === undefined ? undefined : fGet(v, f);
  const name = clean(f.name);
  const title = f.desc && f.desc.length < 48 ? f.desc : name;   // the label: the short description, the name under it
  const off = x === undefined;
  let ctl;
  if (f.type === "b")
    ctl = <button type="button" role="switch" aria-checked={x === 1} aria-label={name} class={`tg${x === 1 ? " on" : ""}`} disabled={off} onClick={() => setField(m, r, f, x ? 0 : 1)} />;
  else if (f.type === "e") {
    const opts = Object.entries(f.values);
    ctl = (
      <select class="asel" aria-label={name} disabled={off} value={x === undefined ? "" : String(x)} onChange={(e) => setField(m, r, f, Number((e.currentTarget as HTMLSelectElement).value))}>
        {x !== undefined && !(String(x) in f.values) ? <option value={String(x)}>Value {x}</option> : null}
        {opts.map(([val, text]) => <option value={val}>{text}</option>)}
      </select>
    );
  } else {
    const max = (1 << width(f)) - 1;
    ctl = (
      <div class="stp">
        <button type="button" class="a" aria-label={`Decrease ${name}`} disabled={off || x === 0} onClick={() => setField(m, r, f, (x ?? 0) - 1)}><Icon name="remove" /></button>
        <b class="mono">{x === undefined ? "—" : x}</b>
        <button type="button" class="a" aria-label={`Increase ${name}`} disabled={off || x === max} onClick={() => setField(m, r, f, (x ?? 0) + 1)}><Icon name="add" /></button>
      </div>
    );
  }
  return (
    <div class="rw noic">
      <span class="lb"><span>{title}<span class="sub">{title === name ? f.desc : name}</span></span></span>
      <span class="c">{ctl}</span>
    </div>
  );
}

function RegGroup({ m, r, open, onToggle }: { m: AdvMap; r: AdvReg; open: boolean; onToggle: () => void }) {
  const k = advKey(m.i2c, m.map, r.addr);
  const changed = advPending.value.has(k);
  const fields = r.fields.filter((f) => OPTION_TYPES.includes(f.type));
  const v = byteOf(k);
  const sum = v === undefined ? "not read" : fields.slice(0, 2).map((f) => {
    const x = fGet(v, f), n = clean(f.name);
    return f.type === "b" ? `${n} ${x ? "on" : "off"}` : f.type === "e" ? `${n} ${f.values[String(x)] ?? x}` : `${n} ${x}`;
  }).join(" · ");
  return (
    <div class={`agrp${changed ? " changed" : ""}`} data-areg={k}>
      <button type="button" class="agh" aria-expanded={open} onClick={onToggle}>
        <span class="mono regaddr">{hx(r.addr)}</span>
        <span class="lb"><b>{r.name}</b><small>{changed ? "changed · " : ""}{fields.length} option{fields.length === 1 ? "" : "s"} · {sum}{v === undefined ? "" : ` · 0x${hx(v)}`}{r.danger === "warn" ? " · resets or powers down the chip" : ""}</small></span>
        <Icon name={open ? "expand_less" : "expand_more"} />
      </button>
      {open ? (
        <div class="grp">
          {r.managed ? <div class="rw note"><span class="ic"><Icon name="info" /></span><span class="lb"><span>The device sets this; your change is lost when it re-applies its settings.</span></span></div> : null}
          {fields.map((f) => <FieldRow key={f.lsb} m={m} r={r} f={f} />)}
        </div>
      ) : null}
    </div>
  );
}

function matches(r: AdvReg, q: string): boolean {
  if (!q) return true;
  const s = q.toLowerCase();
  return r.name.toLowerCase().includes(s) || r.fields.some((f) => OPTION_TYPES.includes(f.type) &&
    (f.name.toLowerCase().includes(s) || f.desc.toLowerCase().includes(s) || Object.values(f.values).some((t) => t.toLowerCase().includes(s))));
}

export function PendingBar({ onApply }: { onApply: () => void }) {
  const n = advPending.value.size;
  if (!n) return null;
  const updating = advBusyUpdating();
  return (
    <div class="pbar" role="region" aria-label="Pending changes">
      <span>{n} register{n === 1 ? "" : "s"} changed{updating ? " · the ADV Controller is updating" : " · lost when the device re-applies"}</span>
      <button type="button" class="btn ghost" onClick={() => { advPending.value = new Map(); }}>Discard</button>
      <button type="button" class="btn pri" disabled={updating} onClick={onApply}>Apply</button>
    </div>
  );
}

const NOTICE_ICON = { bad: { icon: "error" }, good: { icon: "check_circle" } };

export function Notice() {
  const n = notice.value;
  if (!n) return null;
  return <div class="grp"><div class={`rw msg ${n.tone}`} role={n.tone === "bad" ? "alert" : "status"}><span class="ic"><Icon name={NOTICE_ICON[n.tone].icon} /></span><span class="lb"><span>{n.text}</span></span></div></div>;
}

// Read again when the device may have re-applied its ADV state: an adv.* setting, the input, the ADV Controller's
// status or version changed
const reapplySignature = computed(() => {
  const v = device.values.value, i = device.info.value;
  const adv = Object.keys(v).filter((k) => k.startsWith("adv.")).sort().map((k) => `${k}=${v[k]}`).join("&");
  return `${adv}|${i?.input?.active ?? ""}|${i?.adv_controller?.status ?? ""}|${i?.adv_controller?.version ?? ""}`;
});

function useQuery(): Signal<string> { const r = useRef(signal("")); return r.current; }
function useOpen(): Signal<Set<string>> { const r = useRef(signal(new Set<string>())); return r.current; }

export function AdvRegsSection({ desk, chip }: { desk: boolean; chip: "ADV7280" | "ADV7391" }) {
  const rm = useRegmap();
  const query = useQuery(), open = useOpen();
  const c = rm?.chips.find((x) => x.id === chip);
  const canRead = advCanRead();
  const sig = reapplySignature.value;
  useEffect(() => { if (c && canRead) void readMaps(c.maps); }, [c, canRead, sig]);
  if (!canRead) return <div class="rawpad"><div class="grp"><div class="rw msg"><span class="ic"><Icon name="system_update" /></span><span class="lb"><span>Update the ADV Controller (System › Update) to read and change its registers here.</span></span></div></div></div>;
  if (rm === undefined) return <div class="rawpad"><p class="note">Loading the register map…</p></div>;
  if (!rm || !c) return <div class="rawpad"><p class="note">This firmware was built without the register map.</p></div>;
  const q = query.value.trim();
  return (
    <div class="advw advregs">
      <div class="sgrp">
        <div class="asearch"><Icon name="search" />
          <input type="search" aria-label="Search options" placeholder="Search options" autocomplete="off" spellcheck={false} value={query.value}
                 onInput={(e) => { query.value = (e.currentTarget as HTMLInputElement).value; }} />
        </div>
      </div>
      {recording.value ? (
        <div class="grp"><div class="rw msg"><span class="ic"><Icon name="fiber_manual_record" /></span>
          <span class="lb"><span>Recording a macro: Apply adds these changes as steps.</span></span>
          <button type="button" class="btn ghost" onClick={() => { recording.value = null; }}>Stop</button></div></div>
      ) : null}
      <Notice />
      {q && c.maps.every((m) => !optionRegs(m).some((r) => matches(r, q))) ? <div class="sgrp"><div class="sec">Nothing matches “{q}”</div></div> : null}
      {c.maps.map((m) => {
        const regs = optionRegs(m).filter((r) => matches(r, q));
        const st = mapState.value.get(mapKey(m)) ?? "";
        if (q && !regs.length && !st) return null;   // a search hides the maps it does not touch
        return (
          <div class="sgrp" data-amap={mapKey(m)}>
            <div class="sec regsec">
              <span>{MAP_NAMES[m.id] ?? m.id}{st === "reading" ? " · reading…" : st ? ` · ${st}` : ""}</span>
              {st && st !== "reading" ? <button type="button" class="btn ghost" onClick={() => void readMaps([m])}>Retry</button> : null}
            </div>
            <div class="aopts">
              {regs.map((r) => {
                const k = advKey(m.i2c, m.map, r.addr);
                const isOpen = !!q || open.value.has(k);
                return <RegGroup key={k} m={m} r={r} open={isOpen} onToggle={() => { const n = new Set(open.value); if (n.has(k)) n.delete(k); else n.add(k); open.value = n; }} />;
              })}
            </div>
          </div>
        );
      })}
      <PendingBar onApply={() => void applyPending(rm, c.maps)} />
      {desk ? <ConsoleDrawer /> : null}
    </div>
  );
}

export const DecoderSection = ({ desk }: { desk: boolean }) => <AdvRegsSection desk={desk} chip="ADV7280" />;
export const EncoderSection = ({ desk }: { desk: boolean }) => <AdvRegsSection desk={desk} chip="ADV7391" />;

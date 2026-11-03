// Developer › Scaler · TV5725 › Registers: the 677 fields of tv5725.h by name, in one long list by segment; the
// segment buttons are anchors that jump to their part of the list. Values come from a segment read (dev.tv5725_read,
// then GET /api/dev/regs until the answer carries that gen); edits wait in a pending bar and go out as planned
// dev.tv5725_reg triplets (read-modify-write of every byte a field covers), then the segment is read again. Read-only fields (segment 0 status) cannot change; soft resets ask first.
import { signal, computed } from "@preact/signals";
import { useEffect } from "preact/hooks";
import { Icon } from "../../art/icon.tsx";
import { device } from "../../store/app.ts";
import { confirm } from "../../shell/Confirm.tsx";
import { loadRegmap, fieldValue, type Regmap, type TvField } from "../../regs/regmap.ts";
import { planTvWrites } from "../../regs/tv5725.ts";
import { SysCols } from "../system/parts.tsx";

const SEGMENTS = ["Status and misc", "Input format, HD bypass", "Deinterlacer", "VDS, scaling, sync out", "Memory", "ADC, PLL, sync in"];
const log = (m: string) => console.debug("[regs] " + m);

const map = signal<Regmap | null | undefined>(undefined);   // undefined while loading, null: no map in this firmware
const segs = signal<(Uint8Array | null)[]>([null, null, null, null, null, null]);
const pending = signal<Map<string, number>>(new Map());
const query = signal("");
const seg = signal(3);
const msg = signal<string | null>(null);
const reading = signal<number | null>(null);

const hex2 = (n: number) => n.toString(16).toUpperCase().padStart(2, "0");
const where = (f: TvField) => `${f.seg}_${hex2(f.reg)}`;
const bits = (f: TvField) => (f.width === 1 ? `bit ${f.bit}` : `bits ${f.bit}–${f.bit + f.width - 1}`);
const fieldByName = computed(() => new Map((map.value?.tv5725 ?? []).map((f) => [f.name, f])));

// A whole-register 8-bit field shadowed by the named fields of the same register: hidden in the segment list
const aliases = computed(() => {
  const named = new Set<string>(), out = new Set<string>();
  for (const f of map.value?.tv5725 ?? []) if (!(f.bit === 0 && f.width === 8)) named.add(`${f.seg}:${f.reg}`);
  for (const f of map.value?.tv5725 ?? []) if (f.bit === 0 && f.width === 8 && named.has(`${f.seg}:${f.reg}`)) out.add(f.name);
  return out;
});

function current(f: TvField): number | null {
  const p = pending.value.get(f.name);
  if (p !== undefined) return p;
  const s = segs.value[f.seg];
  return s ? fieldValue(s, f) : null;
}

function setPending(f: TvField, v: number): void {
  const max = 2 ** f.width - 1;
  const value = Math.max(0, Math.min(max, v));
  const next = new Map(pending.value);
  const s = segs.value[f.seg];
  if (s && fieldValue(s, f) === value) next.delete(f.name); else next.set(f.name, value);
  pending.value = next;
}

async function readSegment(n: number): Promise<void> {
  reading.value = n;
  msg.value = null;
  try {
    const res = await device.action("dev.tv5725_read", hex2(n));
    if (!res.ok) { msg.value = `Segment ${n} not read (HTTP ${res.status})`; return; }
    const gen = Number((await res.json()).gen);
    for (let i = 0; i < 40; i++) {
      await new Promise((r) => setTimeout(r, 150));
      const r = await device.client.getJson<{ state: string; seg: number; gen: number; hex: string }>("/api/dev/regs");
      if (r.gen !== gen) continue;   // an older answer
      if (r.state === "busy") { msg.value = "The device is busy with a firmware update: read again when it ends."; log(`seg ${n}: busy`); return; }
      if (r.state !== "done") continue;
      const bytes = Uint8Array.from(r.hex.match(/../g) ?? [], (b) => parseInt(b, 16));
      const next = segs.value.slice();
      next[n] = bytes;
      segs.value = next;
      log(`seg ${n}: read (gen ${gen})`);
      return;
    }
    msg.value = `No answer for segment ${n}: try again.`;
  } catch (e) {
    msg.value = `Segment ${n} not read: ${String(e)}`;
  } finally {
    reading.value = null;
  }
}

async function writePending(): Promise<void> {
  const byName = fieldByName.value;
  const list = [...pending.value].map(([n, v]) => [byName.get(n)!, v] as [TvField, number]).filter(([f]) => f);
  if (list.some(([f]) => f.access === "confirm")) {
    const ok = await confirm({ icon: "warning", title: "Write the soft reset bits?", body: "A soft reset blanks the picture for a moment; the signal can drop until the device locks again.", ok: "Write" });
    if (!ok) return;
  }
  for (const n of [...new Set(list.map(([f]) => f.seg))]) {
    const before = segs.value[n];
    await readSegment(n);   // the other bits as they are now, not as at the last read (the firmware may have moved them)
    const s = segs.value[n];
    if (!s || s === before) { msg.value = msg.value ?? `Segment ${n} could not be read again: nothing written.`; return; }
    for (const data of planTvWrites(list.filter(([f]) => f.seg === n), s)) {
      log(`write seg ${n}: ${data}`);
      const res = await device.action("dev.tv5725_reg", data);
      if (!res.ok) { msg.value = `Not written (HTTP ${res.status})`; return; }
    }
    const next = new Map(pending.value);
    for (const [f] of list) if (f.seg === n) next.delete(f.name);
    pending.value = next;
    await readSegment(n);
  }
}

function FieldControl({ f }: { f: TvField }) {
  const v = current(f);
  const off = f.access === "ro" || v === null;
  if (f.width === 1)
    return <button type="button" role="switch" aria-checked={v === 1} aria-label={f.name} class={`tg${v === 1 ? " on" : ""}`} disabled={off} onClick={() => setPending(f, v ? 0 : 1)} />;
  if (f.width <= 12)
    return (
      <div class="stp">
        <button type="button" class="a" aria-label={`Decrease ${f.name}`} disabled={off} onClick={() => setPending(f, (v ?? 0) - 1)}><Icon name="remove" /></button>
        <b class="mono">{v === null ? "—" : v}</b>
        <button type="button" class="a" aria-label={`Increase ${f.name}`} disabled={off} onClick={() => setPending(f, (v ?? 0) + 1)}><Icon name="add" /></button>
      </div>
    );
  return (
    <input class="hexin mono" aria-label={f.name} disabled={off} value={v === null ? "—" : "0x" + v.toString(16).toUpperCase()}
           onChange={(e) => { const n = parseInt((e.currentTarget as HTMLInputElement).value.replace(/^0x/i, ""), 16); if (!Number.isNaN(n)) setPending(f, n); }} />
  );
}

function FieldRow({ f }: { f: TvField }) {
  const dirty = pending.value.has(f.name);
  return (
    <div class={`rw${dirty ? " dirty" : ""}`} data-field={f.name}>
      <span class="ic mono regaddr">{where(f)}</span>
      <span class="lb"><span>{f.name}<span class="sub">{bits(f)}{f.access === "ro" ? " · read-only" : f.access === "confirm" ? " · soft reset" : ""}</span></span></span>
      <span class="c"><FieldControl f={f} /></span>
    </div>
  );
}

function SegmentHead({ n }: { n: number }) {
  const read = !!segs.value[n];
  return (
    <div class="sec regsec" id={`regseg-${n}`}>
      <span>Segment {n} · {SEGMENTS[n]}{read ? "" : " · not read"}</span>
      <button type="button" class="btn ghost sm" aria-label={`Read segment ${n}`} disabled={reading.value !== null} onClick={() => void readSegment(n)}>
        <Icon name="download" />{read ? "Read again" : "Read"}
      </button>
    </div>
  );
}

function FieldList({ fields }: { fields: TvField[] }) {
  return <div class="grp">{fields.map((f) => <FieldRow key={f.name} f={f} />)}</div>;
}

function Results() {
  const q = query.value.trim().toLowerCase();
  const all = map.value?.tv5725 ?? [];
  if (!q)
    return <>{SEGMENTS.map((_, n) => <div class="sgrp" key={n}><SegmentHead n={n} /><FieldList fields={all.filter((f) => f.seg === n && !aliases.value.has(f.name))} /></div>)}</>;
  const hits = all.filter((f) => f.name.toLowerCase().includes(q));
  if (!hits.length) return <div class="sgrp"><div class="sec">No register matches “{query.value}”</div></div>;
  const bySeg = [...new Set(hits.map((f) => f.seg))];
  return <>{bySeg.map((n) => <div class="sgrp" key={n}><SegmentHead n={n} /><FieldList fields={hits.filter((f) => f.seg === n)} /></div>)}</>;
}

function Pending() {
  const n = pending.value.size;
  if (!n) return null;
  return (
    <div class="pbar" role="region" aria-label="Pending changes">
      <span>{n} change{n === 1 ? "" : "s"} · not saved in the profile</span>
      <button type="button" class="btn ghost" onClick={() => { pending.value = new Map(); }}>Discard</button>
      <button type="button" class="btn pri" onClick={() => void writePending()}>Write</button>
    </div>
  );
}

// An anchor: clear the search, then bring that segment's header to the top of the list
function jump(n: number): void {
  seg.value = n;
  query.value = "";
  requestAnimationFrame(() => document.getElementById(`regseg-${n}`)?.scrollIntoView({ block: "start" }));
}

export function RegistersSection({ desk }: { desk: boolean }) {
  useEffect(() => {
    if (map.value === undefined) void loadRegmap((p) => device.client.get(p)).then((m) => { map.value = m; log(m ? `map: ${m.tv5725.length} fields` : "map: none"); });
  }, []);
  if (map.value === undefined) return <div class="rawpad"><p class="note">Loading the register map…</p></div>;
  if (map.value === null) return <div class="rawpad"><p class="note">This firmware was built without the register map.</p></div>;
  const left = (
    <div class="sgrp">
      <div class="asearch regsearch"><Icon name="search" />
        <input type="search" aria-label="Register name" placeholder="Register name (e.g. VDS_HSYNC_RST)" autocomplete="off" spellcheck={false}
               value={query.value} onInput={(e) => { query.value = (e.currentTarget as HTMLInputElement).value; }} />
      </div>
      <div class="sec">Segments</div>
      <div class="grp" role="radiogroup" aria-label="Segment">
        {SEGMENTS.map((name, i) => (
          <button type="button" role="radio" aria-checked={!query.value && seg.value === i} class={`rw lnk${!query.value && seg.value === i ? " sel" : ""}`}
                  onClick={() => jump(i)}>
            <span class="ic mono regaddr">S{i}</span><span class="lb"><span>{name}</span></span>
          </button>
        ))}
      </div>
    </div>
  );
  const right = (
    <div>
      {msg.value ? <div class="grp"><div class="rw msg bad" role="alert"><span class="ic"><Icon name="error" /></span><span class="lb"><span>{msg.value}</span></span></div></div> : null}
      <Results />
      <Pending />
    </div>
  );
  return <div class="tvregs"><SysCols desk={desk} left={left} right={right} /></div>;
}

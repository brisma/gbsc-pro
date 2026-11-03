// Macro editor sheet: steps built from the known options (chip, search, value), a manual register as the fallback (map,
// register, bits), reorder and remove, "Record" to take the next Apply from Decoder or Encoder; saved as w / wm lines
// (ADV Manager .yaml). Lines the page cannot run are kept as they are.
import { signal } from "@preact/signals";
import { Icon } from "../../../art/icon.tsx";
import { device } from "../../../store/app.ts";
import { openSheet, closeSheet, SheetButtons } from "../../../shell/Sheet.tsx";
import { confirm } from "../../../shell/Confirm.tsx";
import { parseAdvYaml, writeAdvYaml } from "../../../regs/yaml.ts";
import { stepsToCommands, type MacroStep } from "../../../regs/adv-model.ts";
import { advKey, type Regmap, type AdvMap, type AdvReg, type AdvField } from "../../../regs/regmap.ts";
import { useRegmap, regmapNow, advValues } from "./AdvRegs.tsx";
import { recording } from "./recorder.ts";
import { hx } from "./adv-io.ts";
import { storeSave, storeDelete, storeText, type StoreItem } from "./store-io.ts";

type EdStep = MacroStep | { kind: "text"; text: string };
type Add = { mode: "option"; chip: number; q: string; pick: { m: AdvMap; r: AdvReg; f: AdvField } | null; x: number }
  | { mode: "manual"; map: number; reg: number; value: number };
const draft = signal<{ item: StoreItem | null; name: string; desc?: string; steps: EdStep[]; add: Add | null; busy: boolean }>({ item: null, name: "", steps: [], add: null, busy: false });
const CHIPS = ["Decoder · ADV7280", "Encoder · ADV7391"];
const MAPN: Record<string, string> = { user_sub_map: "Main", user_sub_map_2: "ACE", vpp_map: "VPP", main: "Main" };
const OPT = ["e", "b", "i"];
const clean = (n: string) => n.replace(/\[.*\]$/, "");
const width = (f: AdvField) => f.msb - f.lsb + 1;
const set = (p: Partial<typeof draft.value>) => { draft.value = { ...draft.value, ...p }; };

const allMaps = (rm: Regmap) => rm.chips.flatMap((c) => c.maps.map((m) => ({ c, m })));
function regAt(rm: Regmap, i2c: number, map: number, reg: number) {
  for (const { c, m } of allMaps(rm)) if (m.i2c === i2c && m.map === map) return { c, m, r: m.regs.find((x) => x.addr === reg) };
  return null;
}
const known = (rm: Regmap, i2c: number, map: number, reg: number) => advValues.value.get(advKey(i2c, map, reg)) ?? regAt(rm, i2c, map, reg)?.r?.def ?? 0;

function choicesOf(f: AdvField): [string, string][] | null {
  if (f.type === "b") return Object.keys(f.values).length ? Object.entries(f.values) : [["0", "Off"], ["1", "On"]];
  if (f.type === "e") return Object.entries(f.values);
  return null;
}

function label(rm: Regmap, s: EdStep): [string, string, string] {
  if (s.kind === "text") return [s.text, "not run on the page", ""];
  const at = regAt(rm, s.i2c, s.map, s.reg), chip = at?.c.id === "ADV7391" ? "Encoder" : "Decoder";
  if (s.kind === "reg") return [at?.r?.name ?? `Register 0x${hx(s.reg)}`, `${chip} · ${at ? MAPN[at.m.id] ?? at.m.id : hx(s.i2c)} · 0x${hx(s.reg)}`, `0x${hx(s.value)}`];
  const f = at?.r?.fields.find((x) => x.lsb === s.lsb);
  const v = f ? (choicesOf(f)?.find(([k]) => Number(k) === s.x)?.[1] ?? String(s.x)) : String(s.x);
  return [f ? clean(f.name) : `bit ${s.lsb}`, `${chip} · ${at?.r?.name ?? ""}`, v];
}

function lines(rm: Regmap): string[] {
  const steps = draft.value.steps;
  const cmds = stepsToCommands(steps.filter((s): s is MacroStep => s.kind !== "text"), rm, advValues.value);
  let i = 0;
  return steps.map((s) => (s.kind === "text" ? s.text : cmds[i++]));
}

async function save(): Promise<void> {
  const rm = regmapNow(), d = draft.value;
  if (!rm || !d.name.trim()) return;
  set({ busy: true });
  const text = writeAdvYaml({ kind: "macro", name: d.name.trim(), description: d.desc, commands: lines(rm).map((t) => ({ text: t, run: true })) });
  const ok = await storeSave("macro", text, `${d.name.trim()}.yaml`, d.item?.id);
  set({ busy: false });
  if (ok) closeSheet();
}

async function remove(): Promise<void> {
  const it = draft.value.item;
  if (!it) return;
  closeSheet();
  if (await confirm({ icon: "delete", title: `Delete ${it.name}?`, body: "The macro is removed from the device. It cannot be undone.", ok: "Delete" })) await storeDelete("macro", it.id);
}

function record(): void {
  recording.value = (changes) => {
    set({ steps: [...draft.value.steps, ...changes.map((c) => ({ kind: "reg" as const, ...c }))] });
    reopen();
  };
  closeSheet();
  device.toast("Recording: set options in Decoder or Encoder and press Apply");
}

function move(i: number): void { const s = draft.value.steps.slice(); [s[i - 1], s[i]] = [s[i], s[i - 1]]; set({ steps: s }); }
function drop(i: number): void { set({ steps: draft.value.steps.filter((_, k) => k !== i) }); }

function OptionAdd({ rm, a }: { rm: Regmap; a: Extract<Add, { mode: "option" }> }) {
  if (a.pick) {
    const { m, r, f } = a.pick, ch = choicesOf(f), max = (1 << width(f)) - 1;
    return (
      <div class="addbox">
        <div class="abh">
          <button type="button" class="ibtn" aria-label="Back to the options" onClick={() => set({ add: { ...a, pick: null } })}><Icon name="arrow_back" /></button>
          <span><b>{clean(f.name)}</b><small>{r.name}</small></span><span class="sp" />
          <button type="button" class="ibtn" aria-label="Close" onClick={() => set({ add: null })}><Icon name="close" /></button>
        </div>
        {ch ? (
          <div class="vlist" role="radiogroup" aria-label="Value">
            {ch.map(([k, t]) => (
              <button type="button" role="radio" aria-checked={Number(k) === a.x} class={`optrow${Number(k) === a.x ? " on" : ""}`} onClick={() => set({ add: { ...a, x: Number(k) } })}>
                <Icon name={Number(k) === a.x ? "radio_button_checked" : "radio_button_unchecked"} /><span>{t}</span><small />
              </button>
            ))}
          </div>
        ) : (
          <div class="rw noic"><span class="lb"><span>Value<span class="sub">0–{max}</span></span></span>
            <div class="stp">
              <button type="button" class="a" aria-label="Decrease value" disabled={a.x <= 0} onClick={() => set({ add: { ...a, x: a.x - 1 } })}><Icon name="remove" /></button>
              <b class="mono">{a.x}</b>
              <button type="button" class="a" aria-label="Increase value" disabled={a.x >= max} onClick={() => set({ add: { ...a, x: a.x + 1 } })}><Icon name="add" /></button>
            </div>
          </div>
        )}
        <div class="row2"><span class="sp" /><button type="button" class="btn pri" onClick={() => set({ steps: [...draft.value.steps, { kind: "field", i2c: m.i2c, map: m.map, reg: r.addr, lsb: f.lsb, x: a.x }], add: null })}><Icon name="add" />Add step</button></div>
      </div>
    );
  }
  const q = a.q.toLowerCase();
  const hits: { m: AdvMap; r: AdvReg; f: AdvField }[] = [];
  for (const m of rm.chips[a.chip].maps) for (const r of m.regs) {
    if (r.readonly || r.danger === "planner") continue;
    for (const f of r.fields) if (OPT.includes(f.type) && (!q || `${f.name} ${f.desc} ${r.name}`.toLowerCase().includes(q))) hits.push({ m, r, f });
  }
  return (
    <div class="addbox">
      <div class="abh"><b>Add an option</b><span class="sp" /><button type="button" class="ibtn" aria-label="Close" onClick={() => set({ add: null })}><Icon name="close" /></button></div>
      <div class="vr" role="radiogroup" aria-label="Chip">
        {CHIPS.map((n, i) => <button type="button" role="radio" aria-checked={a.chip === i} class={a.chip === i ? "on" : ""} onClick={() => set({ add: { ...a, chip: i } })}>{n}</button>)}
      </div>
      <div class="asearch"><Icon name="search" />
        <input type="search" aria-label="Search options" placeholder="Search options" value={a.q} onInput={(e) => set({ add: { ...a, q: (e.currentTarget as HTMLInputElement).value } })} /></div>
      <div class="flist">
        {hits.slice(0, 7).map((h) => (
          <button type="button" class="frow" onClick={() => set({ add: { ...a, pick: h, x: (known(rm, h.m.i2c, h.m.map, h.r.addr) >> h.f.lsb) & ((1 << width(h.f)) - 1) } })}>
            <span><b>{clean(h.f.name)}</b> <small>{h.r.name}{rm.chips[a.chip].maps.length > 1 ? ` · ${MAPN[h.m.id] ?? h.m.id}` : ""}</small></span><Icon name="chevron_right" />
          </button>
        ))}
        {hits.length > 7 ? <p class="note">{hits.length - 7} more: refine the search</p> : null}
      </div>
    </div>
  );
}

function ManualAdd({ rm, a }: { rm: Regmap; a: Extract<Add, { mode: "manual" }> }) {
  const maps = allMaps(rm), { c, m } = maps[a.map], r = m.regs.find((x) => x.addr === a.reg);
  return (
    <div class="addbox">
      <div class="abh"><b>Manual register</b><span class="sp" /><button type="button" class="ibtn" aria-label="Close" onClick={() => set({ add: null })}><Icon name="close" /></button></div>
      <div class="mgrid" role="radiogroup" aria-label="Register map">
        {maps.map((x, i) => <button type="button" role="radio" aria-checked={a.map === i} class={a.map === i ? "on" : ""} onClick={() => set({ add: { ...a, map: i, value: known(rm, x.m.i2c, x.m.map, a.reg) } })}>{x.c.id} {MAPN[x.m.id] ?? x.m.id}</button>)}
      </div>
      <div class="rw noic"><span class="lb"><span>Register<span class="sub">{r ? r.name : "not in the map"}{r?.readonly ? " · read-only" : ""}</span></span></span>
        <div class="stp">
          <button type="button" class="a" aria-label="Previous register" onClick={() => { const g = (a.reg + 255) % 256; set({ add: { ...a, reg: g, value: known(rm, m.i2c, m.map, g) } }); }}><Icon name="remove" /></button>
          <b class="mono">0x{hx(a.reg)}</b>
          <button type="button" class="a" aria-label="Next register" onClick={() => { const g = (a.reg + 1) % 256; set({ add: { ...a, reg: g, value: known(rm, m.i2c, m.map, g) } }); }}><Icon name="add" /></button>
        </div>
      </div>
      <div class="bits">{[7, 6, 5, 4, 3, 2, 1, 0].map((b) => (
        <button type="button" role="switch" aria-checked={((a.value >> b) & 1) === 1} aria-label={`Bit ${b}`} class={(a.value >> b) & 1 ? "on" : ""} onClick={() => set({ add: { ...a, value: a.value ^ (1 << b) } })}>{(a.value >> b) & 1}<i>{b}</i></button>
      ))}</div>
      <div class="row2"><b class="mono">0x{hx(a.value)}</b><span class="sp" />
        <button type="button" class="btn pri" disabled={!!r?.readonly} onClick={() => set({ steps: [...draft.value.steps, { kind: "reg", i2c: m.i2c, map: m.map, reg: a.reg, value: a.value }], add: null })}><Icon name="add" />Add step</button></div>
      <span hidden>{c.id}</span>
    </div>
  );
}

function Body() {
  const rm = useRegmap();
  const d = draft.value;
  if (!rm) return <p class="note">Loading the register map…</p>;
  return (
    <>
      <div class="field"><label for="me-name">Name</label>
        <input id="me-name" class="fin" value={d.name} maxLength={32} placeholder="e.g. Composite sharp" onInput={(e) => set({ name: (e.currentTarget as HTMLInputElement).value })} /></div>
      <div class="field"><label>Steps · applied in order</label>
        <div class="cmdl">
          {d.steps.length ? d.steps.map((s, i) => { const [t, sub, v] = label(rm, s); return (
            <div class="stepr">
              <span class="n">{i + 1}</span><span class="st"><b>{t}</b><small>{sub}</small></span><span class="sv">{v}</span>
              <button type="button" class="ibtn" aria-label={`Move step ${i + 1} up`} disabled={i === 0} onClick={() => move(i)}><Icon name="arrow_upward" /></button>
              <button type="button" class="ibtn" aria-label={`Remove step ${i + 1}`} onClick={() => drop(i)}><Icon name="close" /></button>
            </div>
          ); }) : <p class="note">No steps yet: add the options to change.</p>}
        </div>
      </div>
      {d.add ? (d.add.mode === "option" ? <OptionAdd rm={rm} a={d.add} /> : <ManualAdd rm={rm} a={d.add} />) : (
        <div class="addbar">
          <button type="button" class="btn" onClick={() => set({ add: { mode: "option", chip: 0, q: "", pick: null, x: 0 } })}><Icon name="add" />Add an option</button>
          <button type="button" class="btn ghost" onClick={() => set({ add: { mode: "manual", map: 0, reg: 0, value: known(rm, 0x42, 0, 0) } })}><Icon name="tune" />Manual register</button>
          <span class="sp" />
          <button type="button" class="btn ghost" onClick={record}><Icon name="fiber_manual_record" />Record</button>
        </div>
      )}
      <SheetButtons>
        {d.item ? <button type="button" class="btn ghost danger" onClick={() => void remove()}>Delete</button> : null}
        <button type="button" class="btn ghost" onClick={() => closeSheet()}>Cancel</button>
        <button type="button" class="btn pri" disabled={!d.name.trim() || d.busy} onClick={() => void save()}><Icon name="save" />Save</button>
      </SheetButtons>
    </>
  );
}

function reopen(): void {
  openSheet({ title: draft.value.item ? "Edit macro" : "New macro", icon: "play_circle", body: () => <Body /> });
}

export async function openMacroEditor(item: StoreItem | null): Promise<void> {
  let steps: EdStep[] = [], desc: string | undefined;
  if (item) {
    const p = parseAdvYaml((await storeText("macro", item.id)) ?? "");
    if (p.ok && p.doc.kind === "macro") {
      desc = p.doc.description;
      steps = p.doc.commands.map((c) => (c.run && c.write ? { kind: "reg" as const, ...c.write } : { kind: "text" as const, text: c.text }));
    }
  }
  draft.value = { item, name: item?.name ?? "", desc, steps, add: null, busy: false };
  reopen();
}

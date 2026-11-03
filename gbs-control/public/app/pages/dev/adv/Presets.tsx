// Developer › ADV Controller › Presets and macros: ADV Manager .yaml files stored on the device.
// Presets: the built-in Default (the register map's values) and the stored ones; Apply asks with the register count,
// the skipped registers and the warning for dangerous ones. Macros: Run sends their w / wm lines through the planner.
// New, Import, Export (the stored text unchanged) and Edit for both.
import { signal } from "@preact/signals";
import { useEffect, useRef } from "preact/hooks";
import { Icon } from "../../../art/icon.tsx";
import { device, pred } from "../../../store/app.ts";
import { CAP } from "../../../model/predicates.ts";
import { confirm } from "../../../shell/Confirm.tsx";
import { download } from "../../../shell/files.ts";
import { parseAdvYaml } from "../../../regs/yaml.ts";
import { presetChanges, macroChanges, defaultPreset, withName } from "../../../regs/adv-model.ts";
import { planAdvWrites, type AdvChange } from "../../../regs/planner.ts";
import { advKey, type Regmap } from "../../../regs/regmap.ts";
import { useRegmap, readRegs, advValues, DANGER_TEXT } from "./AdvRegs.tsx";
import { advWrite, advBusyUpdating, logLine, hx } from "./adv-io.ts";
import { storeLists, loadLists, storeText, storeSave, type StoreKind, type StoreItem } from "./store-io.ts";
import { openPresetEditor } from "./PresetEditor.tsx";
import { openMacroEditor } from "./MacroEditor.tsx";

const APPLIED_KEY = "gbsc.advApplied";
const applied = signal<string>((() => { try { return localStorage.getItem(APPLIED_KEY) ?? ""; } catch { return ""; } })());
function setApplied(name: string): void { applied.value = name; try { localStorage.setItem(APPLIED_KEY, name); } catch { /* per browser only */ } }

// Planner, then one question with the count, the skipped registers and the warning, the writes and a read of the maps
// they touched: "done" only when the read shows every written byte, "unconfirmed" otherwise, null when not sent
export async function runChanges(rm: Regmap, title: string, changes: AdvChange[], okWord: string, always: boolean): Promise<"done" | "unconfirmed" | null> {
  if (advBusyUpdating()) { device.toast("The ADV Controller is updating: try again when it ends", "error"); return null; }
  const plan = planAdvWrites(changes, rm);
  const n = new Set(changes.map((c) => `${c.i2c}:${c.map}:${c.reg}`)).size - plan.skipped.length;
  const parts = [`${n} register${n === 1 ? "" : "s"} will be written.`];
  if (plan.skipped.length) parts.push(`Skipped: ${plan.skipped.map((s) => `0x${hx(s.reg.reg)} (${s.reason})`).join(", ")}.`);
  if (plan.dangerous.length) parts.push(`${plan.dangerous.map((d) => `0x${hx(d.reg)}`).join(", ")} can reset or power down the chip: ${DANGER_TEXT}.`);
  if ((always || plan.dangerous.length) && !(await confirm({ icon: plan.dangerous.length ? "warning" : "tune", title, body: parts.join(" "), ok: `${okWord} ${n}` }))) return null;
  for (const r of plan.requests) if (!(await advWrite(r))) { device.toast("Not sent: the device refused the write", "error"); return null; }
  const skipped = new Set(plan.skipped.map((x) => advKey(x.reg.i2c, x.reg.map, x.reg.reg)));
  const written = new Map<string, number>();   // last value per register, as the planner merged them
  for (const c of changes) { const k = advKey(c.i2c, c.map, c.reg); if (!skipped.has(k)) written.set(k, c.value); }
  const read = await readRegs([...written.keys()]);
  return read && [...written].every(([k, v]) => advValues.value.get(k) === v) ? "done" : "unconfirmed";
}

const UNCONFIRMED = "sent, but not confirmed: the ADV Controller did not answer the read or shows other values";

async function applyPreset(rm: Regmap, item: StoreItem | null): Promise<void> {
  let doc = item ? null : defaultPreset(rm);
  if (item) {
    const text = await storeText("preset", item.id);
    const p = text ? parseAdvYaml(text) : null;
    if (!p || !p.ok || p.doc.kind !== "preset") { device.toast(`${item.name}: ${p && !p.ok ? p.error : "not a preset"}`, "error"); return; }
    doc = p.doc;
  }
  const name = item ? item.name : "Default";
  const r = await runChanges(rm, `Apply ${name}?`, presetChanges(doc!, rm), "Apply", true);
  if (r === "done") {
    setApplied(name);
    logLine("ok", `preset ${name} applied`);
    device.toast(`Applied ${name}`);
  } else if (r === "unconfirmed") {
    logLine("err", `preset ${name}: ${UNCONFIRMED}`);
    device.toast(`${name}: ${UNCONFIRMED}`, "error");
  }
}

async function runMacro(rm: Regmap, item: StoreItem): Promise<void> {
  const text = await storeText("macro", item.id);
  await runMacroText(rm, item.name, text);
}

// A built-in macro: the ADV Manager macros the register map carries (what the firmware applies for 2X, Smooth, ACE…)
const builtinText = (commands: string[]) => `name: built-in\ncommands:\n${commands.map((c) => `  - ${c}\n`).join("")}`;

async function runMacroText(rm: Regmap, name: string, text: string | null): Promise<void> {
  const item = { name };
  const p = text ? parseAdvYaml(text) : null;
  if (!p || !p.ok || p.doc.kind !== "macro") { device.toast(`${item.name}: not a macro`, "error"); return; }
  const skipped = p.doc.commands.filter((c) => !c.run).length;
  const r = await runChanges(rm, `Run ${item.name}?`, macroChanges(p.doc), "Run", false);
  if (r === "done") {
    logLine("ok", `macro ${item.name} ran${skipped ? `, ${skipped} line${skipped === 1 ? "" : "s"} not run` : ""}`);
    device.toast(`Ran ${item.name}${skipped ? ` (${skipped} line${skipped === 1 ? "" : "s"} not run)` : ""}`);
  } else if (r === "unconfirmed") {
    logLine("err", `macro ${item.name}: ${UNCONFIRMED}`);
    device.toast(`${item.name}: ${UNCONFIRMED}`, "error");
  }
}

async function exportItem(kind: StoreKind, item: StoreItem): Promise<void> {
  const text = await storeText(kind, item.id);
  if (text === null) { device.toast("Not exported: the file is gone", "error"); return; }
  download(text, `${item.name.replace(/[^\w .()-]+/g, "_")}.yaml`, "application/x-yaml");
}

async function importFile(kind: StoreKind, file: File): Promise<void> {
  const text = withName(await file.text(), file.name);
  const p = parseAdvYaml(text);
  if (!p.ok) { device.toast(`${file.name}: ${p.error}`, "error"); return; }
  if (p.doc.kind !== kind) { device.toast(`${file.name} is a ${p.doc.kind}: import it under ${p.doc.kind === "preset" ? "Presets" : "Macros"}`, "error"); return; }
  await storeSave(kind, text, file.name);
}

function ListHead({ kind, count }: { kind: StoreKind; count: number }) {
  const input = useRef<HTMLInputElement>(null);
  const title = kind === "preset" ? "Presets" : "Macros";
  return (
    <div class="lhead">
      <span class="lt">{title} <small>{count}</small></span><span class="sp" />
      <button type="button" class="lact" onClick={() => (kind === "preset" ? openPresetEditor(null) : openMacroEditor(null))}><Icon name="add" /><span>New {kind}</span></button>
      <button type="button" class="lact" aria-label={`Import ${kind}`} onClick={() => input.current?.click()}><Icon name="upload_file" /><span>Import</span></button>
      <input ref={input} type="file" accept=".yaml,.yml" hidden onChange={(e) => { const f = (e.currentTarget as HTMLInputElement).files?.[0]; (e.currentTarget as HTMLInputElement).value = ""; if (f) void importFile(kind, f); }} />
    </div>
  );
}

const KIND_ICON = { preset: { icon: "bookmark" }, macro: { icon: "play_circle" } };

function ItemRow({ rm, kind, item }: { rm: Regmap; kind: StoreKind; item: StoreItem }) {
  const isApplied = kind === "preset" && applied.value === item.name;
  return (
    <div class="rw prow">
      <span class="ic"><Icon name={KIND_ICON[kind].icon} /></span>
      <span class="lb"><span>{item.name}<span class="sub">{item.description ? `${item.description} · ` : ""}{item.count} {kind === "preset" ? "register" : "command"}{item.count === 1 ? "" : "s"}</span></span>{isApplied ? <span class="pill ac">applied</span> : null}</span>
      <span class="pact">
        <button type="button" class="btn ghost" aria-label={`${kind === "preset" ? "Apply" : "Run"} ${item.name}`} onClick={() => void (kind === "preset" ? applyPreset(rm, item) : runMacro(rm, item))}>{kind === "preset" ? "Apply" : "Run"}</button>
        <button type="button" class="ibtn" aria-label={`Export ${item.name}`} onClick={() => void exportItem(kind, item)}><Icon name="download" /></button>
        <button type="button" class="ibtn" aria-label={`Edit ${item.name}`} onClick={() => (kind === "preset" ? openPresetEditor(item) : openMacroEditor(item))}><Icon name="edit" /></button>
      </span>
    </div>
  );
}

export function PresetsSection(_: { desk: boolean }) {
  const rm = useRegmap();
  // the store exists only with the ADV Controller (404 otherwise); before /api/info the board is not known yet
  const adv = !!device.info.value && (pred.value.caps & CAP.ADV) !== 0;
  useEffect(() => { if (adv) void loadLists(); }, [device.advStore.value.seq, adv]);
  if (rm === undefined) return <div class="rawpad"><p class="note">Loading the register map…</p></div>;
  if (!rm) return <div class="rawpad"><p class="note">This firmware was built without the register map.</p></div>;
  const L = storeLists.value;
  const presets = (
    <div class="sgrp">
      <ListHead kind="preset" count={(L?.preset.length ?? 0) + 1} />
      <div class="grp">
        <div class="rw prow">
          <span class="ic"><Icon name="tune" /></span>
          <span class="lb"><span>Default<span class="sub">The register map's default values</span></span>{applied.value === "Default" ? <span class="pill ac">applied</span> : null}</span>
          <span class="pact"><button type="button" class="btn ghost" aria-label="Apply Default" onClick={() => void applyPreset(rm, null)}>Apply</button></span>
        </div>
        {(L?.preset ?? []).map((p) => <ItemRow key={p.id} rm={rm} kind="preset" item={p} />)}
      </div>
    </div>
  );
  const macros = (
    <div class="sgrp">
      <ListHead kind="macro" count={(L?.macro.length ?? 0) + rm.macros.length} />
      <div class="grp">
        {(L?.macro ?? []).map((m) => <ItemRow key={m.id} rm={rm} kind="macro" item={m} />)}
        {rm.macros.map(([name, commands]) => (
          <div class="rw prow">
            <span class="ic"><Icon name="play_circle" /></span>
            <span class="lb"><span>{name}<span class="sub">built-in · {commands.length} command{commands.length === 1 ? "" : "s"}</span></span></span>
            <span class="pact"><button type="button" class="btn ghost" aria-label={`Run ${name}`} onClick={() => void runMacroText(rm, name, builtinText(commands))}>Run</button></span>
          </div>
        ))}
      </div>
    </div>
  );
  return (
    <div class="advw advpresets">
      {presets}
      {macros}
      <p class="ahelp">Stored on the device. Import and export use the ADV Manager .yaml format: files made with it import as they are.</p>
    </div>
  );
}

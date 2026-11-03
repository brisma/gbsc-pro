// Preset editor sheet: name, description and the values: taken from what is applied now
// (only what differs from Default, every register, one chip) or, for a stored preset, kept; Delete for a stored one
import { signal } from "@preact/signals";
import { Icon } from "../../../art/icon.tsx";
import { device } from "../../../store/app.ts";
import { openSheet, closeSheet, SheetButtons } from "../../../shell/Sheet.tsx";
import { confirm } from "../../../shell/Confirm.tsx";
import { parseAdvYaml, writeAdvYaml, type PresetDoc } from "../../../regs/yaml.ts";
import { presetFromApplied, type PresetScope } from "../../../regs/adv-model.ts";
import { useRegmap, regmapNow, readMaps, advValues } from "./AdvRegs.tsx";
import { storeSave, storeDelete, storeText, type StoreItem } from "./store-io.ts";

type Scope = PresetScope | "keep";
const SCOPES: [Scope, string][] = [["changed", "Only what differs from Default"], ["all", "Every register of both chips"], ["dec", "Decoder only (ADV7280)"], ["enc", "Encoder only (ADV7391)"]];
const draft = signal<{ item: StoreItem | null; name: string; desc: string; scope: Scope; busy: boolean }>({ item: null, name: "", desc: "", scope: "changed", busy: false });

async function save(): Promise<void> {
  const d = draft.value, rm = regmapNow();
  if (!rm || !d.name.trim()) return;
  draft.value = { ...d, busy: true };
  let doc: PresetDoc;
  if (d.scope === "keep" && d.item) {
    const p = parseAdvYaml((await storeText("preset", d.item.id)) ?? "");
    if (!p.ok || p.doc.kind !== "preset") { draft.value = { ...d, busy: false }; return; }
    doc = { ...p.doc, name: d.name.trim(), description: d.desc };
  } else {
    const maps = rm.chips.filter((c) => d.scope !== "dec" && d.scope !== "enc" || c.id === (d.scope === "dec" ? "ADV7280" : "ADV7391")).flatMap((c) => c.maps);
    if (!(await readMaps(maps))) {   // the values applied now, read from the chips: without them nothing is saved
      device.toast("Not saved: the ADV Controller did not answer the read, so the values applied now are unknown", "error");
      draft.value = { ...draft.value, busy: false };
      return;
    }
    doc = presetFromApplied(rm, advValues.value, d.scope as PresetScope, d.name.trim(), d.desc);
  }
  const ok = await storeSave("preset", writeAdvYaml(doc), `${d.name.trim()}.yaml`, d.item?.id);
  draft.value = { ...draft.value, busy: false };
  if (ok) closeSheet();
}

function Body() {
  useRegmap();
  const d = draft.value;
  const scopes: [Scope, string][] = d.item ? [["keep", "Keep its values"], ...SCOPES] : SCOPES;
  return (
    <>
      <div class="field"><label for="pe-name">Name</label>
        <input id="pe-name" class="fin" value={d.name} maxLength={32} placeholder="e.g. NTSC 1X fix" onInput={(e) => { draft.value = { ...draft.value, name: (e.currentTarget as HTMLInputElement).value }; }} /></div>
      <div class="field"><label for="pe-desc">Description</label>
        <input id="pe-desc" class="fin" value={d.desc} maxLength={64} placeholder="what it changes" onInput={(e) => { draft.value = { ...draft.value, desc: (e.currentTarget as HTMLInputElement).value }; }} /></div>
      <div class="field" role="radiogroup" aria-label="Values">
        <label>{d.item ? "Values" : "Take the values applied now"}</label>
        {scopes.map(([k, n]) => (
          <button type="button" role="radio" aria-checked={d.scope === k} class={`optrow${d.scope === k ? " on" : ""}`} onClick={() => { draft.value = { ...draft.value, scope: k }; }}>
            <Icon name={d.scope === k ? "radio_button_checked" : "radio_button_unchecked"} /><span>{n}</span><small />
          </button>
        ))}
      </div>
      <p class="note">Tip: set the options in Decoder and Encoder, press Apply, then save them here.</p>
      <SheetButtons>
        {d.item ? <button type="button" class="btn ghost danger" onClick={() => void remove()}>Delete</button> : null}
        <button type="button" class="btn ghost" onClick={() => closeSheet()}>Cancel</button>
        <button type="button" class="btn pri" disabled={!d.name.trim() || d.busy} onClick={() => void save()}><Icon name="save" />Save</button>
      </SheetButtons>
    </>
  );
}

async function remove(): Promise<void> {
  const it = draft.value.item;
  if (!it) return;
  closeSheet();
  if (await confirm({ icon: "delete", title: `Delete ${it.name}?`, body: "The preset is removed from the device. It cannot be undone.", ok: "Delete" })) await storeDelete("preset", it.id);
}

export function openPresetEditor(item: StoreItem | null): void {
  draft.value = { item, name: item?.name ?? "", desc: item?.description ?? "", scope: item ? "keep" : "changed", busy: false };
  openSheet({ title: item ? "Edit preset" : "New preset", icon: "bookmark", body: () => <Body /> });
}

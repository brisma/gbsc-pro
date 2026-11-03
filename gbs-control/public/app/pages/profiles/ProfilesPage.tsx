// Profiles: one card with Defaults and every saved profile (Load or Save changes, and "more": a menu beside the button
// on PC, a sheet from the bottom on the phone), New / Import / Export all; with no profile yet, how profiles work.
// Then the card of what the device does by itself when a signal arrives. A failure shows in a red row on top.
import { signal } from "@preact/signals";
import { useRef, useState } from "preact/hooks";
import catalogJson from "../../../gen/catalog.json" with { type: "json" };
import { Icon } from "../../art/icon.tsx";
import { device, desk, unsaved } from "../../store/app.ts";
import { profileLabel } from "../../store/profiles.ts";
import { openSheet, closeSheet, SheetButtons } from "../../shell/Sheet.tsx";
import { confirm } from "../../shell/Confirm.tsx";
import { GroupView } from "../../sections/SectionsPage.tsx";
import { Card, GroupBand } from "../../sections/Card.tsx";
import type { Group } from "../../layout/types.ts";
import type { ProfileEntry } from "../../store/device.ts";
import { op, loadProfile, discardChanges, profileError } from "./ops.tsx";
import { exportOne, exportAll, importFiles } from "./transfer.tsx";

const MAX = 36;
const REGIONS = ["NTSC-U", "NTSC-J", "PAL", "PAL60", "None"];
const menuFor = signal<string | null>(null);   // PC: the profile whose menu is open

const WHEN: Group = { rows: [
  { kind: "toggle", ref: "menu:profiles/auto-load", icon: "autorenew" },
  { kind: "toggle", ref: "menu:profiles/load-sets-input", icon: "input" },
  { kind: "inline", ref: "menu:profiles/startup", icon: "power_settings_new" },
] };

const saveActive = (p: ProfileEntry) => void op("/api/profiles/save", { id: p.id });
const regionOf = (p: ProfileEntry) => (p.region && p.region !== "None" && !p.name.endsWith(" " + p.region) ? p.region : "");

// Every action of one profile beyond Load and Save changes; Delete is the last one
interface Act { icon: string; t: string; sub?: string; run: () => void; danger?: boolean }
function actionsOf(p: ProfileEntry): Act[] {
  const active = p.id === device.profiles.value?.active;
  const label = profileLabel(p);
  const acts: Act[] = [];
  if (active && unsaved.value) acts.push({ icon: "history", t: "Discard changes", sub: "Back to what was saved", run: () => void discardChanges() });
  if (!active) acts.push({ icon: "save", t: "Save current settings here", sub: "Keeps its name and its signal", run: async () => {
    if (await confirm({ icon: "save", title: `Save into ${label}?`, body: `The current picture settings replace the ones stored in ${label}. Its name and signal stay.`, ok: "Save" }))
      void op("/api/profiles/save", { id: p.id });
  } });
  acts.push({ icon: "swap_horiz", t: "Replace with current", sub: "Also takes the signal coming in now", run: async () => {
    if (await confirm({ icon: "swap_horiz", title: `Replace ${label}?`, body: "It takes the current settings and the signal coming in now.", ok: "Replace" }))
      void op("/api/profiles/replace", { id: p.id });
  } });
  acts.push({ icon: "edit", t: "Rename", run: () => openRename(p) });
  acts.push({ icon: "content_copy", t: "Duplicate", run: () => void op("/api/profiles/duplicate", { id: p.id }) });
  if (p.fp) acts.push({ icon: "link_off", t: "Forget signal", sub: "Auto-load stops matching this profile", run: () => void op("/api/profiles/clear_fp", { id: p.id }) });
  acts.push({ icon: "download", t: "Export", sub: "One .txt file", run: () => void exportOne(p) });
  acts.push({ icon: "delete", t: "Delete", danger: true, run: async () => {
    if (await confirm({ icon: "delete", title: `Delete ${label}?`, body: "It cannot be undone.", ok: "Delete" })) void op("/api/profiles/delete", { id: p.id });
  } });
  return acts;
}

function MenuItems({ p, done }: { p: ProfileEntry; done: () => void }) {
  return (
    <>
      {actionsOf(p).flatMap((a) => [
        a.danger ? <div class="msep" key={`sep:${a.t}`} /> : null,
        <button type="button" role="menuitem" key={a.t} class={`mi${a.danger ? " danger" : ""}`} aria-label={a.t} onClick={() => { done(); a.run(); }}>
          <Icon name={a.icon} /><span><b>{a.t}</b>{a.sub ? <small>{a.sub}</small> : null}</span>
        </button>,
      ])}
    </>
  );
}

// Phone: the actions come up from the bottom, with the profile named on top
function openActions(p: ProfileEntry, from: EventTarget | null): void {
  openSheet({ title: p.name, icon: "sports_esports", sub: [regionOf(p), p.fp || "No signal stored"].filter(Boolean).join(", "),
    body: () => <div class="mlist" role="menu"><MenuItems p={p} done={closeSheet} /></div> }, from);
}

function openRename(p: ProfileEntry): void {
  const Body = () => {
    const [name, setName] = useState(p.name);
    return (
      <>
        <div class="field"><label for="rn">Name</label><input id="rn" class="in" aria-label="Name" maxLength={24} value={name} onInput={(e) => setName((e.target as HTMLInputElement).value)} /></div>
        <SheetButtons>
          <button type="button" class="btn ghost" onClick={closeSheet}>Cancel</button>
          <button type="button" class="btn pri" disabled={!name.trim()} onClick={() => { closeSheet(); void op("/api/profiles/rename", { id: p.id, name: name.trim() }); }}>Rename</button>
        </SheetButtons>
      </>
    );
  };
  openSheet({ title: "Rename", icon: "edit", body: () => <Body /> });
}

// New profile: name with the console catalog, region, and when a profile already matches the signal: Replace it /
// Keep both (the menu's same-signal prompt)
function openNew(): void {
  const pal = Number(device.info.value?.signal?.src_hz10 ?? 600) < 550;
  const Body = () => {
    const [name, setName] = useState("");
    const [region, setRegion] = useState(pal ? "PAL" : "NTSC-U");
    const same = device.profiles.value?.profiles.find((x) => x.match);
    const n = name.trim();
    const saveAs = () => { closeSheet(); void op("/api/profiles/saveas", { name: n, region }); };
    return (
      <>
        <div class="field"><label for="np">Name</label>
          <input id="np" class="in" aria-label="Name" maxLength={24} placeholder="Name or a console below" value={name} onInput={(e) => setName((e.target as HTMLInputElement).value)} /></div>
        <div class="field"><label>Or pick a console</label>
          <div class="chips">{(catalogJson as string[]).map((c) => <button type="button" class={`chip${name === c ? " on" : ""}`} onClick={() => setName(c)}>{c}</button>)}</div></div>
        <div class="field"><label id="rgl">Region</label>
          <div class="chips" role="radiogroup" aria-labelledby="rgl">{REGIONS.map((r) => <button type="button" role="radio" aria-checked={region === r} class={`chip${region === r ? " on" : ""}`} onClick={() => setRegion(r)}>{r}</button>)}</div></div>
        {same ? <div class="warnbox"><Icon name="info" /><span><b>Same signal as {profileLabel(same)}.</b> Keep both, or replace that profile?</span></div> : null}
        <SheetButtons>
          <button type="button" class="btn ghost" onClick={closeSheet}>Cancel</button>
          {same ? <button type="button" class="btn" disabled={!n} onClick={() => { closeSheet(); void op("/api/profiles/replace", { id: same.id, name: n, region }); }}>Replace it</button> : null}
          <button type="button" class="btn pri" disabled={!n} onClick={saveAs}>{same ? "Keep both" : "Save"}</button>
        </SheetButtons>
      </>
    );
  };
  openSheet({ title: "New profile", icon: "add_circle_outline", body: () => <Body /> });
}

function ErrorRow() {
  if (!profileError.value) return null;
  return <div class="rw note bad perr" role="alert"><span class="ic"><Icon name="error" /></span><span class="ds">{profileError.value}</span></div>;
}

function DefaultsRow() {
  const on = !device.profiles.value?.active;
  return (
    <div class="rw prow" data-row="profile:defaults">
      <span class="ic"><Icon name="restart_alt" /></span>
      <span class="lb"><span>Defaults</span>{on ? <span class="tag">Active</span> : null}</span>
      <span class="ds">The factory picture settings, not a saved profile.</span>
      <div class="c"><button type="button" class="btn" aria-label="Load Defaults" onClick={() => loadProfile("", "Defaults")}>Load</button></div>
    </div>
  );
}

function ProfileRow({ p }: { p: ProfileEntry }) {
  const on = p.id === device.profiles.value?.active;
  const uns = on && unsaved.value;
  const label = profileLabel(p);
  const region = regionOf(p);
  const open = desk.value && menuFor.value === p.id;
  const close = () => { menuFor.value = null; };
  // PC: beside the name; phone: on a line of their own over the signal
  const tags = [
    on ? <span class="tag" key="a">Active</span> : null,
    uns && desk.value ? <span class="tag" key="u">unsaved changes</span> : null,
    p.match ? <span class="tag ok" key="m">Signal match</span> : null,
  ].filter(Boolean);
  return (
    <div class="rw prow" data-row={`profile:${p.id}`}>
      <span class="ic"><Icon name="sports_esports" /></span>
      <span class="lb"><span>{p.name}</span>{region ? <small>{region}</small> : null}{desk.value ? tags : null}</span>
      <span class="ds">{desk.value || !tags.length ? null : <span class="tgs">{tags}</span>}{p.fp || "No signal stored"}</span>
      <div class="c">
        {uns && desk.value ? <button type="button" class="btn" aria-label={`Discard the changes to ${label}`} onClick={() => void discardChanges()}><Icon name="history" />Discard</button> : null}
        {on ? (uns ? <button type="button" class="btn pri" onClick={() => saveActive(p)}><Icon name="save" />{desk.value ? "Save changes" : "Save"}</button> : null)
          : <button type="button" class="btn" aria-label={`Load ${label}`} onClick={() => loadProfile(p.id, label)}>Load</button>}
        <button type="button" class="btn ico" aria-label={`Actions for ${label}`} aria-haspopup="menu" aria-expanded={open}
                onClick={(e) => { if (desk.value) menuFor.value = open ? null : p.id; else openActions(p, e.currentTarget); }}><Icon name="more_horiz" /></button>
      </div>
      {open ? <><div class="scrim fix" onClick={close} /><div class="pmenu" role="menu" onKeyDown={(e) => { if (e.key === "Escape") close(); }}><MenuItems p={p} done={close} /></div></> : null}
    </div>
  );
}

const STEPS: [string, string, string, string][] = [   // icon, title, the line under it on PC, the line on the phone
  ["tune", "Set up the picture", "for one console", "For one console, in Settings."],
  ["save", "Save it as a profile", "New profile", "With New, above this list."],
  ["autorenew", "It comes back by itself", "when that signal returns", "When that signal returns, with Auto-load on."],
];

function HowItWorks() {
  return (
    <>
      <GroupBand title="How profiles work" />
      {desk.value ? (
        <div class="steps">
          {STEPS.flatMap(([icon, t, d], i) => [
            i ? <Icon name="arrow_forward" key={`a${i}`} /> : null,
            <div key={t}><span class="sib"><Icon name={icon} /></span><b>{t}</b><small>{d}</small></div>,
          ])}
        </div>
      ) : STEPS.map(([icon, t, , d], i) => (
        <div class="rw" key={t}><span class="ic"><Icon name={icon} /></span><span class="lb"><span>{i + 1}. {t}</span></span><span class="ds">{d}</span></div>
      ))}
    </>
  );
}

export function ProfilesPage() {
  const file = useRef<HTMLInputElement>(null);
  const pick = () => file.current?.click();
  const list = device.profiles.value?.profiles ?? [];
  const count = list.length;
  const isDesk = desk.value;
  const add = <button type="button" class="btn pri" onClick={openNew}><Icon name="add" />{isDesk ? "New profile" : "New"}</button>;
  return (
    <>
      <input ref={file} type="file" data-import multiple accept=".txt,.zip,text/plain,application/zip" style="display:none"
             onChange={(e) => { const f = (e.target as HTMLInputElement).files; if (f?.length) void importFiles([...f]); (e.target as HTMLInputElement).value = ""; }} />
      <Card icon="sports_esports" title="Profiles" sub={count ? `${count} saved of ${MAX}` : `None saved yet, room for ${MAX}`} section="profiles"
            actions={isDesk ? [add,
              <button type="button" class="btn" onClick={pick}><Icon name="upload_file" />Import</button>,
              <button type="button" class="btn" onClick={() => void exportAll()} disabled={!count}><Icon name="download" />Export all</button>] : [add]}>
        <ErrorRow />
        <DefaultsRow />
        {count ? list.map((p) => <ProfileRow key={p.id} p={p} />) : <HowItWorks />}
        {isDesk ? null : (
          <div class="more">
            <span class="ic"><Icon name="description" /></span><span>Profiles as files</span>
            <button type="button" onClick={pick}>Import</button><button type="button" onClick={() => void exportAll()} disabled={!count}>Export all</button>
          </div>
        )}
      </Card>
      <Card icon="autorenew" title="When a signal arrives" sub="What the device does by itself" section="profiles-auto">
        <GroupView g={WHEN} />
      </Card>
    </>
  );
}

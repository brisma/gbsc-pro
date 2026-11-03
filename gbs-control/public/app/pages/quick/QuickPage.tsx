// Quick: the card of shortcuts (the menu's Quick items by default, kept per browser; Edit with stars and up/down and
// "Add from all settings") and the card that loads a profile
import { signal, effect } from "@preact/signals";
import { Icon } from "../../art/icon.tsx";
import { device } from "../../store/app.ts";
import { readPref, writePref } from "../../store/persist.ts";
import { allNodes, nodeById, labelFor, parentOf } from "../../model/menu.ts";
import { hashOf } from "../../shell/router.ts";
import { RowView, isRowVisible } from "../../rows/Row.tsx";
import { homeOf } from "../../rows/Frame.tsx";
import { profileLabel } from "../../store/profiles.ts";
import { loadProfile } from "../profiles/ops.tsx";
import { Card, GroupBand } from "../../sections/Card.tsx";
import type { Row } from "../../layout/types.ts";
import { cleanPins, addPin, removePin, movePin, rowForNode, pinnable } from "./pins.ts";

export const pins = signal<string[]>(cleanPins(readPref<unknown>("gbsc.quickPins", null)));
effect(() => writePref("gbsc.quickPins", pins.value));
const editing = signal(false);

const QUICK_PROFILE = "quick/profile";   // the TV's Quick Profile row: here the card under the shortcuts does its job
const ALL_SETTINGS: Row = { kind: "link", icon: "tune", label: "All settings", sub: "Input, Scaler, Picture, Output", to: "#/settings" };

function pinRow(id: string): Row | null {
  const n = nodeById(id);
  return n ? rowForNode(n) : null;
}

function EditRow({ id, first, last }: { id: string; first: boolean; last: boolean }) {
  const label = id === QUICK_PROFILE ? "Profile" : labelFor(id);
  const row = pinRow(id);
  return (
    <div class="rw" data-row={`edit:${id}`}>
      <span class="ic"><Icon name={row?.icon ?? "sports_esports"} /></span>
      <span class="lb"><span>{label}</span></span>
      <span class="c">
        <button type="button" class="ibtn" aria-label={`Move ${label} up`} disabled={first} onClick={() => { pins.value = movePin(pins.value, id, -1); }}><Icon name="keyboard_arrow_up" /></button>
        <button type="button" class="ibtn" aria-label={`Move ${label} down`} disabled={last} onClick={() => { pins.value = movePin(pins.value, id, 1); }}><Icon name="keyboard_arrow_down" /></button>
        <button type="button" class="ibtn star on" aria-label={`Unpin ${label}`} onClick={() => { pins.value = removePin(pins.value, id); }}><Icon name="star" fill /></button>
      </span>
    </div>
  );
}

function AddList() {
  const homes = new Set(pins.value.map((id) => homeOf(id)));   // a pinned Quick row already shows its setting
  const offered = allNodes().filter((n) => pinnable(n) && !pins.value.includes(n.id) && !homes.has(n.id) && isRowVisible(rowForNode(n)!));
  return (
    <>
      <GroupBand title="Add from all settings" />
      {offered.map((n) => {
        const par = parentOf(n.id);
        const ctx = par && parentOf(par.id)?.id !== "" ? par.label : "";   // "Decoder" for a decoder row
        const name = ctx ? `${n.label} (${ctx})` : n.label;
        return (
          <div class="rw" data-row={`add:${n.id}`}>
            <span class="ic"><Icon name={rowForNode(n)!.icon} /></span>
            <span class="lb"><span>{n.label}</span></span>
            {ctx ? <span class="ds">{ctx}</span> : null}
            <span class="c"><button type="button" class="ibtn star" aria-label={`Pin ${name}`} onClick={() => { pins.value = addPin(pins.value, n.id); }}><Icon name="star" /></button></span>
          </div>
        );
      })}
    </>
  );
}

function Shortcuts() {
  const ed = editing.value;
  const shown = pins.value.filter((id) => id !== QUICK_PROFILE);
  return (
    <Card icon="bolt" title="Shortcuts" sub="The settings you use most" cls="quick" section="quick"
          actions={[<button type="button" class="btn" onClick={() => { editing.value = !ed; }}><Icon name={ed ? "check" : "edit"} />{ed ? "Done" : "Edit"}</button>]}>
      {ed
        ? shown.map((id, i) => <EditRow key={id} id={id} first={i === 0} last={i === shown.length - 1} />)
        : shown.map((id) => { const r = pinRow(id); return r ? <RowView key={id} row={r} quick /> : null; })}
      {ed ? <AddList /> : <RowView row={ALL_SETTINGS} />}
    </Card>
  );
}

function LoadProfile() {
  const pj = device.profiles.value;
  const list = [...(pj?.profiles ?? [])].sort((a, b) => b.lastused - a.lastused).slice(0, 5);
  const all = hashOf({ page: "profiles", section: null, id: null });
  return (
    <Card icon="sports_esports" title="Load a profile" sub="The pictures saved for your consoles" section="quick-profiles"
          actions={[<a class="btn" href={all}>All profiles<Icon name="chevron_right" /></a>]}>
      {list.length ? list.map((p) => {
        const on = p.id === pj?.active;
        return (
          <div class="rw list" data-row={`profile:${p.id}`}>
            <span class="ic"><Icon name="sports_esports" /></span>
            <span class="lb"><span>{profileLabel(p)}</span>{on ? <span class="tag">Active</span> : null}</span>
            <span class="ds">{p.fp || "no signal stored"}</span>
            <div class="c">{on ? null : <button type="button" class="btn" aria-label={`Load ${profileLabel(p)}`} onClick={() => loadProfile(p.id, profileLabel(p))}>Load</button>}</div>
          </div>
        );
      }) : (
        <div class="rw list" data-row="profile:none">
          <span class="ic"><Icon name="info" /></span>
          <span class="lb"><span>No profiles yet</span></span>
          <span class="ds">Set up the picture for a console, then save it from Profiles.</span>
          <div class="c"><a class="btn" href={all}><Icon name="add" />New profile</a></div>
        </div>
      )}
    </Card>
  );
}

export function QuickPage() {
  return <><Shortcuts /><LoadProfile /></>;
}

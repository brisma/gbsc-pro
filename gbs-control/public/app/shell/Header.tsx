// Header, one row: logo or page title, the update button (PC), the state chip, Remote (PC), the mode switch.
// Notices follow it only when there is something to say: no link to the device, a profile with unsaved changes.
import { Logo } from "../art/logo.tsx";
import { Icon } from "../art/icon.tsx";
import { device, offer, unsaved } from "../store/app.ts";
import { route, navigate, hashOf } from "./router.ts";
import { pageById } from "../layout/pages.ts";
import { ModeSwitch } from "./ModeSwitch.tsx";
import { openRemote } from "./Remote.tsx";
import { chainNow } from "./Chain.tsx";
import { profileLabel } from "../store/profiles.ts";
import { activeProfile, discardChanges } from "../pages/profiles/ops.tsx";

// The one dot of the app: the video signal, or the link to the device when that is what is wrong
function StateChip({ desk }: { desk: boolean }) {
  const c = device.connection.value;
  const st = chainNow().model.state;
  const text = c === "connecting" ? "Connecting…" : c === "reconnecting" ? "Reconnecting…" : c === "offline" ? "Offline"
    : st === "ok" ? "Stable" : st === "search" ? "Searching inputs" : "No signal";
  // the phone's header has room for one word
  const short = desk ? text : text === "Searching inputs" ? "Searching" : text === "Reconnecting…" ? "Offline" : text;
  return (
    <a class="sta" href={hashOf({ page: "status", section: null, id: null })} role="status" aria-label={`Signal: ${text}`}>
      <span class={`led ${c === "connecting" ? "off" : st}`} /><span class="stt">{short}</span>
    </a>
  );
}

export function Header({ desk }: { desk: boolean }) {
  const r = route.value;
  const title = pageById(r.page)?.title ?? "";
  const onUpdate = r.page === "system" && r.section === "update";
  return (
    <header class="hd">
      {r.page === "quick" && !desk ? <h1><Logo height={22} /></h1> : (
        <h1 class="ttl">
          {r.page === "dev" && !desk   // the phone reaches Developer from a System tab: the way back
            ? <button type="button" class="ibtn back" aria-label="Back to System" onClick={() => navigate("system")}><Icon name="arrow_back" /></button>
            : null}
          {title}
        </h1>
      )}
      <span class="sp" />
      {desk && offer.value && !onUpdate ? <a class="btn upd" href={hashOf({ page: "system", section: "update", id: null })}><Icon name="system_update_alt" />Update available</a> : null}
      <StateChip desk={desk} />
      {desk ? <button type="button" class="rpill" onClick={openRemote}><Icon name="settings_remote" fill />Remote</button> : null}
      <ModeSwitch long={desk} />
    </header>
  );
}

async function saveProfile(): Promise<void> {
  const id = device.profiles.value?.active;
  if (!id) return;
  const r = await device.client.post("/api/profiles/save", { id });
  if (!r.ok) device.toast(r.status === 409 ? "The device is busy: try again" : `Not saved (HTTP ${r.status})`, "error");
}

export function Notices() {
  const c = device.connection.value;
  const off = c === "reconnecting" || c === "offline";
  const prof = activeProfile();
  const uns = unsaved.value;   // on every page, Profiles too (its row of the profile has the same two buttons)
  if (!off && !uns) return null;
  return (
    <>
      {off ? <div class="notice bad" role="alert"><Icon name="cloud_off" />{c === "offline" ? "Device offline. Retrying… changes wait in a queue." : "Connection lost. Retrying… changes wait in a queue."}</div> : null}
      {uns ? (
        <div class="notice" role="status">
          <Icon name="bookmark" /><span>{prof ? profileLabel(prof) : "The profile"} has unsaved changes</span><span class="sp" />
          <button type="button" class="btn sm" aria-label="Discard changes" onClick={() => void discardChanges()}><Icon name="history" />Discard</button>
          <button type="button" class="btn pri sm" onClick={() => void saveProfile()}><Icon name="save" />Save profile</button>
        </div>
      ) : null}
    </>
  );
}

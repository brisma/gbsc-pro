// System › Update. Three states: up to date; an update on offer (group Available: Release notes and Install);
// installing (group Installing: progress from s:status.update, the bar kept in place). Then what is installed
// (GBS-Control; Pro: the ADV Controller, Update when the firmware carries a newer image), Auto check and Last result
// (Advanced), the channel the checks follow (the setting system.update_channel: picking one writes it and checks)
// and the upload of a file whose version is read before asking (ota-update.cpp, update-device.cpp, api-web.cpp).
// An older Stable offered to a unit on a Beta that chose Stable shows in Available with its warning; the header
// button and the badges announce newer versions only. Check now is the button of the card header.
import { useEffect, useRef } from "preact/hooks";
import { signal } from "@preact/signals";
import { Icon } from "../../art/icon.tsx";
import { device, mode, board, offer } from "../../store/app.ts";
import { Frame } from "../../rows/Frame.tsx";
import { RowView, isRowVisible } from "../../rows/Row.tsx";
import { runAction, Seg } from "../../rows/controls.tsx";
import { REPO } from "../../shell/Footer.tsx";
import { confirm } from "../../shell/Confirm.tsx";
import { compareVersions, readImageVersion, readAdvImageVersion } from "../../model/version.ts";
import { CHANNELS, channelLabel, reasonWords, lastResultText, checkResultText, busyText } from "../../texts/update.ts";
import type { Row } from "../../layout/types.ts";
import { SysGroup, Progress, MsgRow } from "./parts.tsx";

interface Job { kind: "ota" | "upload"; version: string; fw: string }
const job = signal<Job | null>(null);                                  // an install or upload this page started
const note = signal<{ text: string; tone: "bad" | "good" } | null>(null);
const checking = signal(false);
const log = (m: string) => console.debug("[update] " + m);
const UPLOAD_REASON_WAIT_MS = 2500;
const NOTE_ICON = { bad: "error", good: "check_circle" };   // @icons

// The TV menu's Update rows (menu.json ids) with their icons
const ROWS: Record<string, Row> = {
  download: { kind: "live", ref: "menu:system/update/found-on", icon: "downloading" },
  upload: { kind: "live", ref: "menu:system/update/found-on", icon: "upload" },
  offer: { kind: "action", ref: "menu:system/update/install", icon: "new_releases" },
  current: { kind: "live", ref: "menu:system/update/gbs-control", icon: "memory" },
  file: { kind: "action", label: "Upload firmware", icon: "upload_file" },
  adv: { kind: "live", ref: "menu:system/update/adv-controller", icon: "developer_board" },
  check: { kind: "action", ref: "menu:system/update/check-now", icon: "sync" },
  last: { kind: "live", ref: "menu:system/update/last-check", icon: "history" },
  channel: { kind: "live", ref: "menu:system/update/channel", icon: "alt_route" },
};
const AUTO_CHECK: Row = { kind: "inline", ref: "menu:system/update/auto-check", icon: "event_repeat" };
const fw = () => String(device.info.value?.fw ?? "");
// The store keeps the last status values: cleared before an operation so its answer is the next one that arrives
const clearStatus = (k: "available" | "progress") => { device.update.value = { ...device.update.value, [k]: "" }; };

function start(kind: Job["kind"], version: string): void {
  note.value = null;
  clearStatus("progress");
  job.value = { kind, version, fw: fw() };
  log(`${kind} ${version}: started (running ${fw()})`);
}

async function installOffer(version: string, older: boolean, channel: string): Promise<void> {
  const ok = await confirm(older
    ? { icon: "history", title: "Install an older version?", ok: "Install", body: `You have ${fw()}, ${channelLabel(channel)} has ${version}. Settings and profiles stay; an older version may not know every setting.` }
    : { icon: "system_update_alt", title: `Install ${version}?`, ok: "Install", body: `GBS-Control downloads ${version} and restarts. Settings and profiles stay.` });
  if (!ok) return;
  start("ota", version);
  // "older": the question above was about an older build; the device refuses an offer that changed meanwhile
  if (!(await runAction("system.install_update", older ? "older" : undefined))) job.value = null;
}

// An ADV Controller image: stored on the device (LittleFS), then offered like the built-in one; removed once installed
async function uploadAdv(file: File, v: string): Promise<void> {
  if (board.value !== "pro") { note.value = { text: "This board has no ADV Controller.", tone: "bad" }; return; }
  if (!(await confirm({ icon: "developer_board", title: `Upload ADV Controller ${v}?`, ok: "Upload",
    body: `${file.name} is stored on the GBSC PRO and offered as the ADV Controller update. It is removed once installed.` }))) return;
  note.value = null;
  const form = new FormData();
  form.append("image", file, file.name);
  let res: Response, body: any = {};
  try {
    res = await device.client.exclusive("/api/system/adv-image", form);
    body = await res.json().catch(() => ({}));
  } catch (e) {
    log(`adv image: ${String(e)}`);
    note.value = { text: "The upload stopped: the device did not answer.", tone: "bad" };
    return;
  }
  log(`adv image: HTTP ${res.status} ${JSON.stringify(body)}`);
  if (!res.ok) {
    note.value = { text: res.status === 503 ? busyText(res.headers.get("X-GBSC-Busy")) : `The ADV Controller image was refused: ${advImageReason(String(body.error ?? res.status))}.`, tone: "bad" };
    return;
  }
  const running = String(device.info.value?.adv_controller?.build ?? "").toUpperCase();
  if (running && running === String(body.build).toUpperCase()) {
    note.value = { text: `The ADV Controller already runs ${body.version}.`, tone: "good" };
    return;
  }
  if (await confirm({ icon: "developer_board", title: `Update the ADV Controller to ${body.version}?`, ok: "Update",
    body: "The picture may stop for a few seconds. The image stays on the device until it is installed." })) {
    await runAction("system.update_adv_controller");
  } else {
    note.value = { text: `ADV Controller ${body.version} stored: Update is offered below.`, tone: "good" };
  }
}

const ADV_IMAGE_REASONS: Record<string, string> = {
  format: "not an ADV Controller image", size: "the file is incomplete or too big", crc: "the file is damaged",
  space: "not enough space on the device", busy: "an update is running", no_adv: "this board has no ADV Controller",
  forbidden: "not allowed from this address", write: "the device could not store it",
};
const advImageReason = (code: string) => ADV_IMAGE_REASONS[code] ?? code;

async function upload(file: File): Promise<void> {
  const adv = await readAdvImageVersion(file);
  if (adv !== null) { log(`file ${file.name}: ADV Controller ${adv}`); return uploadAdv(file, adv); }
  const v = await readImageVersion(file);
  const older = v !== null && compareVersions(v, fw()) < 0;
  log(`file ${file.name}: version ${v ?? "not found"}${older ? " (older)" : ""}`);
  const ok = await confirm(v === null
    ? { icon: "upload_file", title: "Install this file?", ok: "Install", body: `The version could not be read from ${file.name}. Install it anyway? The device checks the image before using it.` }
    : older
      ? { icon: "history", title: "Install an older version?", ok: "Install", body: `You have ${fw()}, the file is ${v}. Settings and profiles stay; an older version may not know every setting.` }
      : { icon: "upload_file", title: `Install ${v}?`, ok: "Install", body: `${file.name} replaces GBS-Control ${fw()}. Settings and profiles stay.` });
  if (!ok) return;
  start("upload", v ?? file.name);
  const form = new FormData();
  form.append("firmware", file, file.name);
  let res: Response;
  try { res = await device.client.exclusive("/api/system/update", form); } catch (e) {
    log(`upload: ${String(e)}`);
    job.value = null;
    note.value = { text: "The upload stopped: the device did not answer.", tone: "bad" };
    return;
  }
  log(`upload: HTTP ${res.status}${res.headers.get("X-GBSC-Busy") ? " (" + res.headers.get("X-GBSC-Busy") + ")" : ""}`);
  if (res.status === 202 || res.ok) return;   // the device verifies, then reports on the WebSocket
  if (res.status === 503) { job.value = null; note.value = { text: busyText(res.headers.get("X-GBSC-Busy")), tone: "bad" }; return; }
  if (res.status === 409) { job.value = null; return; }
  // the device's reason (format, space, memory...) follows on the WebSocket: wait for it before showing the HTTP code
  const mine = job.value;
  window.setTimeout(() => {
    if (job.value !== mine) return;
    job.value = null;
    note.value = { text: `The upload failed (HTTP ${res.status}).`, tone: "bad" };
  }, UPLOAD_REASON_WAIT_MS);
}

async function checkNow(): Promise<void> {
  clearStatus("available");
  checking.value = true;
  if (!(await runAction("system.check_update"))) checking.value = false;
}

// The channel is a setting: written, then checked at once so the answer for it is on the page
async function pickChannel(ch: string, cur: string): Promise<void> {
  if (ch === cur) return;
  const r = await device.writeSettings([["system.update_channel", CHANNELS.indexOf(ch as (typeof CHANNELS)[number])]]);
  if (r.ok) await checkNow();
}

async function updateAdv(): Promise<void> {
  const img = device.info.value?.adv_image;
  const from = img?.source === "file" ? `the uploaded image ${img.version}` : `the image built into this firmware${img?.version ? " (" + img.version + ")" : ""}`;
  if (!(await confirm({ icon: "developer_board", title: "Update the ADV Controller?", ok: "Update",
    body: `The ADV Controller gets ${from}. The picture may stop for a few seconds.` }))) return;
  await runAction("system.update_adv_controller");
}

function JobRow({ j, progress }: { j: Job; progress: string }) {
  if (progress === "done") return <MsgRow icon="check_circle" tone="good" text={`${j.version} installed: the device restarts`} />;
  const pct = /^\d+$/.test(progress) ? Number(progress) : 0;
  return (
    <Frame row={j.kind === "upload" ? ROWS.upload : ROWS.download} label={`Installing ${j.version}`} sub="Keep the GBSC PRO on until it restarts." reason={null} wide noHelp>
      <Progress pct={pct} label={`Installing ${j.version}`} />
    </Frame>
  );
}

function AdvRow() {
  const a = device.info.value?.adv_controller;
  if (board.value !== "pro" || !a) return null;
  const st = String(a.update ?? "idle");
  const state = a.status !== "ok" ? (a.status === "no_reply" ? "not answering" : String(a.status))
    : st === "running" ? "updating…" : st === "done" ? "updated"
    : st.startsWith("failed") ? `update failed (${st.slice(7)})` : a.update_available ? "update available" : "up to date";
  return (
    <Frame row={ROWS.adv} label="ADV Controller" sub="The firmware of the decoder board." reason={null} noHelp>
      <span class="val">{a.status === "ok" ? a.version : ""}<small>{state}</small></span>
      {a.update_available && st !== "running" ? <button type="button" class="btn" onClick={() => void updateAdv()}>Update</button> : null}
    </Frame>
  );
}

function ChannelRow() {
  const n = Number(device.values.value["system.update_channel"] ?? -1);
  const cur = CHANNELS[n] ?? "";
  return (
    <Frame row={ROWS.channel} label="Channel" reason={null} wide noHelp
           sub={cur === "beta" ? "Beta: test versions too. The newest of Beta and Stable is offered." : "Stable: only finished versions are offered."}>
      <Seg label="Channel" opts={CHANNELS.map((c) => ({ label: channelLabel(c), on: c === cur, pick: () => void pickChannel(c, cur) }))} />
    </Frame>
  );
}

// The card header's button: not while something installs
export function UpdateActions() {
  if (job.value) return null;
  return <button type="button" class="btn" disabled={checking.value} onClick={() => void checkNow()}><Icon name="sync" />{checking.value ? "Checking…" : "Check now"}</button>;
}

export function UpdateSection(_: { desk: boolean }) {
  const input = useRef<HTMLInputElement>(null);
  const upd = device.update.value;
  const info = device.info.value;
  const j = job.value;
  const off = info?.update?.offer;
  const older = !!off?.older;
  const o = older ? String(off?.version ?? "") : offer.value;   // an older Stable is offered here only, never announced

  // Outcomes on the WebSocket: an error ends the job; a new firmware running ends it too
  useEffect(() => {
    if (job.value && upd.progress.startsWith("error:")) {
      const code = upd.progress.slice(6);
      log(`${job.value.kind} failed: ${code}`);
      note.value = { text: `The ${job.value.kind === "upload" ? "upload" : "update"} failed: ${reasonWords(code)}.`, tone: "bad" };
      job.value = null;
    }
  }, [upd.progress]);
  useEffect(() => { if (job.value && info?.fw && info.fw !== job.value.fw) { log(`now running ${info.fw}`); job.value = null; } }, [info?.fw]);
  useEffect(() => {
    if (checking.value && upd.available) { checking.value = false; device.toast(checkResultText(upd.available), upd.available.startsWith("error:") ? "error" : "info"); }
  }, [upd.available]);

  return (
    <>
      {j ? <SysGroup title="Installing"><JobRow j={j} progress={upd.progress} /></SysGroup>
        : o ? (
          <SysGroup title="Available">
            <Frame row={ROWS.offer} label={`GBS-Control ${o}`} reason={null} noHelp wide cls="bt"
                   sub={older ? `${channelLabel(off?.channel ?? "")} channel, older than the ${fw()} you run: the way back to Stable. Settings and profiles stay.`
                     : `The device downloads it and restarts. Settings and profiles stay.${off?.channel ? ` ${channelLabel(off.channel)} channel.` : ""}`}>
              <a class="btn ghost" href={`${REPO}/releases`} target="_blank" rel="noopener">Release notes</a>
              <button type="button" class="btn pri" onClick={() => void installOffer(o, older, off?.channel ?? "")}><Icon name="system_update_alt" />Install</button>
            </Frame>
          </SysGroup>
        ) : null}
      <SysGroup title="Installed">
        {note.value ? <MsgRow icon={NOTE_ICON[note.value.tone]} tone={note.value.tone} text={note.value.text} /> : null}
        <Frame row={ROWS.current} label="GBS-Control" sub="The firmware of the scaler board." reason={null} noHelp>
          <span class="val">{info?.fw ?? ""}{o || j ? null : <small>up to date</small>}</span>
        </Frame>
        <AdvRow />
      </SysGroup>
      {isRowVisible(AUTO_CHECK) || mode.value !== "easy" ? (
        <SysGroup title="Checks">
          <RowView row={AUTO_CHECK} />
          {mode.value !== "easy" ? (
            <Frame row={ROWS.last} label="Last check" reason={null}><span class="val">{lastResultText(String(info?.update?.last ?? ""))}</span></Frame>
          ) : null}
        </SysGroup>
      ) : null}
      <SysGroup title="Channel"><ChannelRow /></SysGroup>
      <SysGroup title="From a file">
        <Frame row={ROWS.file} label="Upload firmware" sub="A GBS-Control .bin or .bin.gz, or an ADV Controller .bin. The version is read before installing." reason={null} noHelp>
          <button type="button" class="btn" disabled={!!j} onClick={() => input.current?.click()}><Icon name="upload_file" />Choose file</button>
        </Frame>
        <input ref={input} type="file" accept=".bin,.gz" hidden
               onChange={(e) => { const el = e.target as HTMLInputElement; const f = el.files?.[0]; el.value = ""; if (f) void upload(f); }} />
      </SysGroup>
    </>
  );
}

export function updateSummary(): string {
  if (job.value) return `Installing ${job.value.version}, the device restarts at the end`;
  return offer.value ? `Version ${offer.value} is available` : "Up to date";
}

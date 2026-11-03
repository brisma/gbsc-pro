// System › Update. Three states: up to date; an update on offer (group Available: Release notes and Install);
// installing (group Installing: progress from s:status.update, the bar kept in place). Then what is installed
// (GBS-Control; Pro: the ADV Controller, Update when the firmware carries a newer image), Auto check and Last result
// (Advanced), other channels and the upload of a file whose version is read before asking (ota-update.cpp,
// update-device.cpp, api-web.cpp). Check now is the button of the card header.
import { useEffect, useRef } from "preact/hooks";
import { signal } from "@preact/signals";
import { Icon } from "../../art/icon.tsx";
import { device, mode, board, offer } from "../../store/app.ts";
import { Frame } from "../../rows/Frame.tsx";
import { RowView, isRowVisible } from "../../rows/Row.tsx";
import { runAction, Seg } from "../../rows/controls.tsx";
import { REPO } from "../../shell/Footer.tsx";
import { confirm } from "../../shell/Confirm.tsx";
import { compareVersions, readImageVersion } from "../../model/version.ts";
import { CHANNELS, channelLabel, reasonWords, lastResultText, checkResultText, busyText } from "../../texts/update.ts";
import type { Row } from "../../layout/types.ts";
import { SysGroup, Progress, MsgRow } from "./parts.tsx";

interface Job { kind: "ota" | "upload"; version: string; fw: string }
const job = signal<Job | null>(null);                                  // an install or upload this page started
const note = signal<{ text: string; tone: "bad" | "good" } | null>(null);
const picked = signal<string | null>(null);                           // another channel looked at
const checking = signal(false);
const log = (m: string) => console.debug("[update] " + m);
const UPLOAD_REASON_WAIT_MS = 2500;
const NOTE_ICON = { bad: "error", good: "check_circle" };   // @icons

// The TV menu's Update rows (menu.json ids) with their icons
const ROWS: Record<string, Row> = {
  download: { kind: "live", ref: "menu:system/update/offer", icon: "downloading" },
  upload: { kind: "live", ref: "menu:system/update/offer", icon: "upload" },
  offer: { kind: "action", ref: "menu:system/update/install", icon: "new_releases" },
  current: { kind: "live", ref: "menu:system/update/gbs-control", icon: "memory" },
  file: { kind: "action", label: "Upload firmware", icon: "upload_file" },
  adv: { kind: "live", ref: "menu:system/update/adv-controller", icon: "developer_board" },
  check: { kind: "action", ref: "menu:system/update/check-now", icon: "sync" },
  last: { kind: "live", ref: "menu:system/update/last-result", icon: "history" },
  channel: { kind: "live", ref: "menu:system/update/channel", icon: "alt_route" },
  manual: { kind: "live", ref: "menu:system/update/other-channels/offer", icon: "science" },
};
const AUTO_CHECK: Row = { kind: "inline", ref: "menu:system/update/auto-check", icon: "event_repeat" };
const fw = () => String(device.info.value?.fw ?? "");
// The store keeps the last status values: cleared before an operation so its answer is the next one that arrives
const clearStatus = (k: "available" | "channelCheck" | "progress") => { device.update.value = { ...device.update.value, [k]: "" }; };

function start(kind: Job["kind"], version: string): void {
  note.value = null;
  clearStatus("progress");
  job.value = { kind, version, fw: fw() };
  log(`${kind} ${version}: started (running ${fw()})`);
}

async function installOffer(version: string, manual: boolean, older: boolean, channel: string): Promise<void> {
  const ok = await confirm(older
    ? { icon: "history", title: "Install an older version?", ok: "Install", body: `You have ${fw()}, ${channelLabel(channel)} has ${version}. Settings and profiles stay; an older version may not know every setting.` }
    : { icon: "system_update_alt", title: `Install ${version}?`, ok: "Install", body: `GBS-Control downloads ${version} and restarts. Settings and profiles stay.` });
  if (!ok) return;
  start("ota", version);
  if (!(await runAction("system.install_update", manual ? "manual" : undefined))) job.value = null;
}

async function upload(file: File): Promise<void> {
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

async function pickChannel(ch: string, build: string): Promise<void> {
  if (ch === build) { picked.value = null; return; }
  picked.value = ch;
  clearStatus("channelCheck");
  await runAction("system.check_channel", ch);
}

async function updateAdv(): Promise<void> {
  if (!(await confirm({ icon: "developer_board", title: "Update the ADV Controller?", ok: "Update",
    body: "The ADV Controller gets the image built into this firmware. The picture may stop for a few seconds." }))) return;
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

function ChannelRows({ build }: { build: string }) {
  const cur = picked.value ?? build;
  const res = device.update.value.channelCheck;
  const m = picked.value ? /^([a-z]+):(.+)$/.exec(res) : null;
  const manual = device.info.value?.update?.manual;
  let row = null;
  if (picked.value) {
    const ch = picked.value;
    if (!res) row = <MsgRow icon="sync" text={`Checking ${channelLabel(ch)}…`} />;
    else if (res.startsWith("error:")) row = <MsgRow icon="error" tone="bad" text={`Check failed: ${reasonWords(res.slice(6))}`} />;
    else if (m && m[2] === "none") row = <MsgRow icon="check_circle" text={`${channelLabel(ch)} has the version you run`} />;
    else if (m) {
      const v = m[2];
      const older = manual?.version === v ? !!manual.older : compareVersions(v, fw()) < 0;
      row = (
        <Frame row={ROWS.manual} label={`GBS-Control ${v}`} sub={`${channelLabel(m[1])}${older ? ", older than yours" : ""}`} reason={null} noHelp>
          <button type="button" class="btn" disabled={!!job.value} onClick={() => void installOffer(v, true, older, m[1])}>Install</button>
        </Frame>
      );
    }
  }
  return (
    <>
      <Frame row={ROWS.channel} label="Channel" sub={build ? `This firmware follows ${channelLabel(build)}.` : undefined} reason={null} wide noHelp>
        <Seg label="Channel" opts={CHANNELS.map((c) => ({ label: channelLabel(c), on: c === cur, pick: () => void pickChannel(c, build) }))} />
      </Frame>
      {row}
    </>
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
  const o = offer.value;
  const off = info?.update?.offer;

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
                   sub={`The device downloads it and restarts. Settings and profiles stay.${off?.channel ? ` ${channelLabel(off.channel)} channel.` : ""}`}>
              <a class="btn ghost" href={`${REPO}/releases`} target="_blank" rel="noopener">Release notes</a>
              <button type="button" class="btn pri" onClick={() => void installOffer(o, false, !!off?.older, off?.channel ?? "")}><Icon name="system_update_alt" />Install</button>
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
            <Frame row={ROWS.last} label="Last result" reason={null}><span class="val">{lastResultText(String(info?.update?.last ?? ""))}</span></Frame>
          ) : null}
        </SysGroup>
      ) : null}
      <SysGroup title="Channel"><ChannelRows build={String(info?.update?.channel ?? "")} /></SysGroup>
      <SysGroup title="From a file">
        <Frame row={ROWS.file} label="Upload firmware" sub="A .bin or .bin.gz file. The version is read before installing." reason={null} noHelp>
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

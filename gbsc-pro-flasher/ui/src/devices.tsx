// USB in three steps: the device, the firmware (dropped, picked or found next to the app), then write; the end step
// depends on the chip.
import { useEffect } from "preact/hooks";
import { signal, computed } from "@preact/signals";
import { Icon } from "../../../gbs-control/public/app/art/icon.tsx";
import { api, errText, onDevices, onDrop, pickFile, type FlashResult } from "./api.ts";
import { busy, confirm, failure, host, note, pct, run, setHost } from "./store.ts";
import { Step } from "./steps.tsx";
import { advCheckFile, defaultDevice, endText, keepLast, flashEnabled, flashQuestions, preselect, sizeOnlyWarning, type CandidatesRow, type DeviceRow, type EraseChoice, type FileRow, type LastFlash } from "./state.ts";

const devices = signal<DeviceRow[]>([]);
const selected = signal<string | null>(null);
const pickingDevice = signal(false);
const file = signal<FileRow | null>(null);
const fileFromUser = signal(false);
const pickingFile = signal(false);
const fileError = signal<string | null>(null);          // a dropped or picked file that could not be read
const candidates = signal<CandidatesRow | null>(null);
const erase = signal<EraseChoice>("none");
const stub = signal(true);
const last = signal<(LastFlash & { result: FlashResult | null; erasedOnly: boolean; error: string | null; erase: EraseChoice }) | null>(null);
const advCheck = signal<string | null>(null);

const device = computed(() => devices.value.find((d) => d.port === selected.value) ?? null);
const usable = (d: DeviceRow | null) => !!d && !d.refuse && (d.kind === "esp" || d.kind === "adv");

const DEV_ICON: Record<string, string> = { esp: "memory", adv: "developer_board", advConsole: "developer_board" }; // @icons
const ERASES: { id: EraseChoice; label: string }[] = [
  { id: "none", label: "Keep everything" }, { id: "user", label: "Erase settings and profiles" },
  { id: "wifi", label: "Erase WiFi settings" }, { id: "all", label: "Erase everything" },
];

function choose(rows: DeviceRow[]) {
  devices.value = rows;
  selected.value = defaultDevice(rows, selected.value);
  if (!fileFromUser.value) file.value = preselect(candidates.value, device.value);
  dropStaleLast(false);
}

function dropStaleLast(byUser: boolean) {
  if (!keepLast(last.value, device.value, byUser)) { last.value = null; advCheck.value = null; }
}

async function load(path: string) {
  // another file starts a new flash: the end step (and its check of the flashed file) goes
  fileError.value = null;
  try { file.value = await api.readFile(path); fileFromUser.value = true; pickingFile.value = false; last.value = null; advCheck.value = null; } catch (e) { note(errText(e)); fileError.value = errText(e); pickingFile.value = true; }   // the open step shows why
}

async function flash() {
  const d = device.value!, f = file.value;
  for (const q of flashQuestions(d, f, erase.value)) if (!(await confirm(q.title, q.body, q.ok))) return;
  last.value = null; advCheck.value = null;
  const how = d.kind === "esp" ? erase.value : "none";   // the ADV Controller has no erase; the choice stays from an ESP8266 job
  const r = await run("flash", () => api.flash(d.port, f?.path ?? null, how, !!f?.bySizeOnly));
  last.value = { kind: d.kind, ok: !!r, result: r, erasedOnly: !f, file: f, port: d.port, ports: devices.value.map((x) => x.port),
                 error: r ? null : failure.value, erase: how };
}

async function checkAdv() {
  const f = advCheckFile(last.value)!;   // the flashed file: the file step may show another one after the unplug
  const ok = await run("check", () => api.advVerify(host.value, f.path));
  advCheck.value = ok === null ? null : ok ? `The ADV Controller runs ${f.version}.` : "The ADV Controller still runs another image: flash it again.";
}

function end(): string {
  const l = last.value;
  if (!l) return "";
  return endText({ kind: l.kind, ok: l.ok, checked: l.result?.checked ?? null, seconds: l.result?.seconds ?? 0, erasedOnly: l.erasedOnly, error: l.error, erase: l.erase });
}

function DeviceList() {
  return (
    <div class="stack">
      {!devices.value.length && <div class="empty"><Icon name="usb_off" /><span>No GBSC device on USB. Connect the GBSC's USB port, or hold the ADV Controller's button while plugging its USB-C cable.</span></div>}
      {devices.value.map((x) => (
        <button class={`dev${x.port === selected.value ? " on" : ""}${x.refuse ? " no" : ""}`} disabled={!!busy.value}
                onClick={() => { selected.value = x.port; if (usable(x)) pickingDevice.value = false; if (!fileFromUser.value) file.value = preselect(candidates.value, x); dropStaleLast(true); }}>
          <Icon name={DEV_ICON[x.kind] ?? "usb"} />
          <span><b>{x.label}</b><small class="mono">{x.port}</small></span>
        </button>
      ))}
      {device.value?.refuse && <div class="warnbox"><Icon name="warning" /><span>{device.value.refuse}</span></div>}
      {device.value?.confirm && <div class="warnbox"><Icon name="warning" /><span>{device.value.confirm}</span></div>}
      {candidates.value?.translocated && <div class="warnbox"><Icon name="warning" /><span>macOS runs this app from a temporary place, so it cannot see the firmware files next to it. Move the app out of the download folder (or run xattr -dr com.apple.quarantine on it) and open it again.</span></div>}
      {candidates.value?.fromZip && <div class="warnbox"><Icon name="warning" /><span>Windows started this program from inside the zip, so the firmware files are not next to it. Close it, right-click the zip, choose Extract All, and open the program from the extracted folder.</span></div>}
    </div>
  );
}

function FilePick() {
  const f = file.value, d = device.value;
  return (
    <div class="stack">
      <div class="drop">
        <Icon name="upload_file" />
        <span>{f ? <><b>{f.name}</b><br />{f.kind === "esp" ? "GBS-Control" : f.kind === "adv" ? "ADV Controller" : "Unknown"}{f.version ? ` ${f.version}` : ""}</> : "Drop a firmware file here"}</span>
        <button class="btn wide" disabled={!!busy.value} onClick={async () => { const p = await pickFile("Firmware", ["bin", "gz"]); if (p) load(p); }}><Icon name="folder_open" />Choose a file</button>
      </div>
      {fileError.value && <div class="warnbox"><Icon name="error" /><span>{fileError.value}</span></div>}
      {d?.kind === "esp" && <button class="btn wide ghost" onClick={() => { file.value = null; fileFromUser.value = true; pickingFile.value = false; }}><Icon name="ink_eraser" />No file: erase only</button>}
    </div>
  );
}

export function Devices() {
  useEffect(() => {
    api.candidates().then((c) => { candidates.value = c; api.devices().then(choose); }).catch((e) => note(errText(e)));
    api.stubPresent().then((s) => (stub.value = s));
    const u1 = onDevices(choose), u2 = onDrop(load);
    return () => { u1.then((f) => f()); u2.then((f) => f()); };
  }, []);
  const d = device.value, f = file.value;
  const s1 = usable(d) && !pickingDevice.value ? "done" : "now";
  const fileOk = (f && d && f.kind === d.kind) || (!f && fileFromUser.value && d?.kind === "esp");
  const s2 = s1 !== "done" ? "wait" : fileOk && !pickingFile.value ? "done" : "now";
  const s3 = s2 !== "done" ? "wait" : last.value?.ok ? "done" : "now";   // a failed write stays open for the retry
  const mismatch = f && d && usable(d) && f.kind !== d.kind;
  return (
    <div class="steps">
      <Step n={1} title="Device" state={s1} summary={d ? `${d.label} on ${d.port}` : ""} onChange={() => (pickingDevice.value = true)}>
        <DeviceList />
      </Step>
      <Step n={2} title="Firmware" state={s2} onChange={() => (pickingFile.value = true)}
            summary={s2 === "wait" ? "Choose the device first" : f ? `${f.name} · ${f.version ?? "no version"}${fileFromUser.value ? "" : ", found next to the app"}` : "No file: erase only"}>
        <FilePick />
        {mismatch && <div class="warnbox"><Icon name="error" /><span>This file is for the {f!.kind === "esp" ? "GBS-Control (ESP8266)" : "ADV Controller"}, not for the selected device.</span></div>}
        {f?.bySizeOnly && <div class="warnbox"><Icon name="warning" /><span>{sizeOnlyWarning(f)}</span></div>}
      </Step>
      <Step n={3} title={!f && fileFromUser.value && d?.kind === "esp" ? "Erase" : "Write"} state={s3} onChange={() => (last.value = null)}
            summary={s3 === "wait" ? "Then write it" : last.value?.ok ? "Done" : ""}>
        <div class="stack">
          {d?.kind === "esp" && (
            <div class="choices" role="radiogroup" aria-label="Before writing">
              {ERASES.map((e) => <button role="radio" aria-checked={erase.value === e.id} class={erase.value === e.id ? "on" : ""} disabled={!!busy.value || (!f && e.id === "none")} onClick={() => (erase.value = e.id)}>{e.label}</button>)}
            </div>
          )}
          {busy.value === "flash" && <div class="bar"><i style={{ width: `${pct.value ?? 0}%` }} /></div>}
          <button class="btn pri wide big" disabled={!!busy.value || !flashEnabled(d, f, erase.value)} onClick={flash}>
            <Icon name="bolt" />{busy.value === "flash" ? `Writing ${pct.value ?? 0} %` : f ? "Flash" : "Erase"}
          </button>
          <p class="hint">{d?.kind === "adv" ? "About 10 s. The ADV Controller's bootloader gives no checksum: the version is checked over WiFi after." : stub.value ? "About 25 s, verified on the chip." : "Slow mode: the fast flasher file (esp8266-stub.json) is missing. About 90 s, not verified."}</p>
        </div>
      </Step>
      <Step n="end" title="Done" state={last.value ? "now" : "wait"} summary="What to do with the cable comes here">
        <div class="stack">
          <div class={last.value?.ok ? "okbox" : "warnbox"}><Icon name={last.value?.ok ? "check_circle" : "error"} /><span>{end()}</span></div>
          {advCheckFile(last.value) && (
            <div class="field">
              <label>GBSC address for the check</label>
              <div class="row"><input value={host.value} onInput={(e) => setHost((e.target as HTMLInputElement).value)} placeholder="gbscontrol.local or 192.168.x.x" />
                <button class="btn" disabled={!host.value || !!busy.value} onClick={checkAdv}><Icon name="verified" />Check</button></div>
              {advCheck.value && <p class="hint">{advCheck.value}</p>}
            </div>
          )}
        </div>
      </Step>
    </div>
  );
}

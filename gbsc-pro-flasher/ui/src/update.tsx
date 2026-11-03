// Update over WiFi in three steps: the GBSC, the GBS-Control image (preselected from the release folder), install;
// then the ADV Controller from the image inside GBS-Control when the unit offers it.
import { useEffect } from "preact/hooks";
import { signal } from "@preact/signals";
import { Icon } from "../../../gbs-control/public/app/art/icon.tsx";
import { api, errText, onDrop, pickFile } from "./api.ts";
import { busy, confirm, failure, host, note, pct, run } from "./store.ts";
import { isDowngrade, sizeOnlyWarning, type FileRow } from "./state.ts";
import { Step } from "./steps.tsx";
import { SafeModeNote, UnitPicker, connect, unit, unitSummary } from "./unit-picker.tsx";

const file = signal<FileRow | null>(null);
const pickingUnit = signal(false);
const pickingFile = signal(false);
const result = signal<string | null>(null);
const fileError = signal<string | null>(null);          // a dropped or picked file that cannot be installed

async function load(path: string) {
  fileError.value = null;
  try {
    const f = await api.readFile(path);
    if (f.kind !== "esp") { fileError.value = "Over WiFi only GBS-Control images are installed; the ADV Controller updates from GBS-Control."; pickingFile.value = true; return; }
    file.value = f; pickingFile.value = false; result.value = null;   // another image: the last result goes
  } catch (e) { note(errText(e)); fileError.value = errText(e); pickingFile.value = true; }   // the open step shows why
}

/** The end step's failure line: the job's own reason when there is one. */
const didNotFinish = (what: string) => `${what} did not finish: ${failure.value ?? "see the message below."}`;

async function install() {
  const u = unit.value!, f = file.value!;
  const warn = sizeOnlyWarning(f);   // the unit installs any ESP8266 image: an upstream build would replace the PRO firmware
  if (warn && !(await confirm("Install this file?", warn, "Install anyway"))) return;
  const older = isDowngrade(u.fw, f.version);
  const ok = await confirm(older ? "Install an older version?" : `Install ${f.version ?? f.name}?`,
    older ? `The GBSC runs ${u.fw}, the file is ${f.version}. Settings and profiles stay; an older version may not know every setting.`
          : `${f.name} replaces GBS-Control ${u.fw}. Settings and profiles stay.`, "Install");
  if (!ok) return;
  result.value = null;
  const v = await run("update", () => api.update(host.value, f.path));
  result.value = v ? `The GBSC restarted with GBS-Control ${v}.` : didNotFinish("The update");
  if (v) connect(host.value);
}

async function installAdv() {
  if (!(await confirm("Update the ADV Controller?", "The GBSC copies the ADV Controller image it carries; the picture stops for about 20 seconds.", "Update"))) return;
  const v = await run("update-adv", () => api.updateAdv(host.value));
  result.value = v !== null ? `The ADV Controller runs ${v}.` : didNotFinish("The ADV Controller update");
  if (v) connect(host.value);
}

export function Update() {
  useEffect(() => {
    if (!file.value) api.candidates().then((c) => { if (c.espWifi && !file.value) file.value = c.espWifi; });
    const u = onDrop(load);
    return () => { u.then((f) => f()); };
  }, []);
  const u = unit.value, f = file.value;
  const s1 = u && !pickingUnit.value ? "done" : "now";
  const s2 = s1 !== "done" ? "wait" : f && !pickingFile.value ? "done" : "now";
  const s3 = s2 !== "done" ? "wait" : "now";
  return (
    <div class="steps">
      <Step n={1} title="GBSC" state={s1} summary={u ? unitSummary(u) : ""} onChange={() => { pickingUnit.value = true; result.value = null; }}>
        <UnitPicker onConnected={() => (pickingUnit.value = false)} />
        {u && <button class="btn pri wide" onClick={() => (pickingUnit.value = false)}><Icon name="check" />Use {host.value}</button>}
      </Step>
      <Step n={2} title="GBS-Control image" state={s2} onChange={() => (pickingFile.value = true)}
            summary={s2 === "wait" ? "Choose the GBSC first" : f ? `${f.name} · ${f.version ?? "version not found"}` : ""}>
        <div class="drop">
          <Icon name="system_update" />
          <span>{f ? <><b>{f.name}</b><br />GBS-Control {f.version ?? "(version not found)"}</> : "Drop a GBS-Control image (.bin or .bin.gz)"}</span>
          <button class="btn wide" disabled={!!busy.value} onClick={async () => { const p = await pickFile("GBS-Control image", ["bin", "gz"]); if (p) load(p); }}><Icon name="folder_open" />Choose a file</button>
        </div>
        {fileError.value && <div class="warnbox"><Icon name="error" /><span>{fileError.value}</span></div>}
      </Step>
      <Step n={3} title="Install" state={s3} summary="Then install it">
        <div class="stack">
          <SafeModeNote />
          {busy.value === "update" && <div class="bar"><i style={{ width: `${pct.value ?? 0}%` }} /></div>}
          <button class="btn pri wide big" disabled={!u || !f || !!busy.value} onClick={install}><Icon name="system_update" />{busy.value === "update" ? `Installing ${pct.value ?? 0} %` : "Install"}</button>
          {u?.advUpdateAvailable && <button class="btn wide" disabled={!!busy.value} onClick={installAdv}><Icon name="developer_board" />Update the ADV Controller</button>}
          <p class="hint">The GBSC checks the image, restarts, and the flasher reads the version it came back with.</p>
        </div>
      </Step>
      <Step n="end" title="Done" state={result.value ? "now" : "wait"} summary="The version after the restart comes here">
        {result.value?.includes("did not finish")
          ? <div class="warnbox"><Icon name="error" /><span>{result.value}</span></div>
          : <div class="okbox"><Icon name="check_circle" /><span>{result.value}</span></div>}
      </Step>
    </div>
  );
}

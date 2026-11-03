// Backup and profiles: download a backup, restore one (checked with the device's rules first), profile files
import { signal } from "@preact/signals";
import { Icon } from "../../../gbs-control/public/app/art/icon.tsx";
import { api, errText, pickFile, pickSave, type ProfileRow } from "./api.ts";
import { busy, confirm, failure, host, note, runOk } from "./store.ts";
import { restoreProblem } from "./state.ts";
import { SafeModeNote, UnitPicker, unit, unitSummary } from "./unit-picker.tsx";

const profiles = signal<ProfileRow[] | null>(null);
const profilesFor = signal("");   // the address the list was read from: another unit never shows (or exports) it
const outcome = signal<{ ok: boolean; text: string } | null>(null);   // the last action's result, on the page

/** The result line of an action: in the log and on the page. */
function tell(ok: boolean, text: string) { note(text); outcome.value = { ok, text }; }
/** A job that threw: its reason (run() already logged it). */
function failed() { outcome.value = { ok: false, text: failure.value ?? "It did not finish: see the message below." }; }
const today = () => new Date().toISOString().slice(0, 10);

async function download() {
  const p = await pickSave(`gbsc-backup-${host.value.replace(/[:/\\]/g, "_")}-${today()}.txt`);   // a :port is no file name on Windows
  if (!p) return;
  outcome.value = null;
  if (await runOk("backup", () => api.backup(host.value, p))) tell(true, `Saved ${p}`); else failed();
}

async function restore() {
  const p = await pickFile("GBSC backup", ["txt"], "docs");
  if (!p) return;
  outcome.value = null;
  let text: string;
  try { text = await api.readText(p); } catch (e) { tell(false, errText(e)); return; }
  const problem = restoreProblem(text);
  if (problem) { tell(false, problem); return; }
  if (!(await confirm("Restore this backup?", "The GBSC's settings and profiles are replaced by the file's, then it restarts.", "Restore"))) return;
  if (await runOk("restore", () => api.restore(host.value, text))) { profiles.value = null; tell(true, "Restored. The GBSC restarts."); } else failed();
}

async function loadProfiles() {
  const h = host.value;
  try { profiles.value = await api.profiles(h); profilesFor.value = h; } catch (e) { tell(false, errText(e)); }
}

async function exportOne(p: ProfileRow) {
  const path = await pickSave(`${p.name.replace(/[/:' ]/g, "_")}.gbscprofile.txt`);
  if (!path) return;
  outcome.value = null;
  if (await runOk("export", () => api.profileExport(host.value, p.id, path))) tell(true, `Saved ${path}`); else failed();
}

async function importOne() {
  const path = await pickFile("GBSC profile", ["txt"], "docs");
  if (!path) return;
  outcome.value = null;
  if (await runOk("import", () => api.profileImport(host.value, path))) { tell(true, "Profile imported."); loadProfiles(); } else failed();
}

export function Backup() {
  const ready = !!unit.value && !busy.value;
  const full = ready && !unit.value?.safeMode;   // safe mode: only the backup download answers
  const list = profilesFor.value === host.value && unit.value ? profiles.value : null;
  return (
    <div class="steps page">
      <div class="card">
        <div class="sh"><b>GBSC</b></div>
        {unit.value ? <div class="sum">{unitSummary(unit.value)}</div> : null}
        <UnitPicker />
        <SafeModeNote />
      </div>
      <div class="card">
        <div class="sh"><b>Backup</b></div>
        <div class="pair">
          <button class="btn" disabled={!ready} onClick={download}><Icon name="download" />Download backup</button>
          <button class="btn" disabled={!full} onClick={restore}><Icon name="settings_backup_restore" />Restore a backup</button>
        </div>
      </div>
      <div class="card">
        <div class="sh"><b>Profiles</b><button class="lnk" disabled={!full} onClick={loadProfiles}><Icon name="refresh" />Load</button></div>
        {list && !list.length && <p class="hint">No profiles saved on this GBSC.</p>}
        {list?.map((p) => (
          <div class="prow"><span><b>{p.name}</b>{p.region ? <small> {p.region}</small> : null}</span>
            <button class="btn" disabled={!full} onClick={() => exportOne(p)}><Icon name="file_save" />Export</button></div>
        ))}
        <button class="btn wide" disabled={!full} onClick={importOne}><Icon name="upload" />Import a profile file</button>
      </div>
      {outcome.value && (
        <div class={outcome.value.ok ? "okbox" : "warnbox"}><Icon name={outcome.value.ok ? "check_circle" : "error"} /><span>{outcome.value.text}</span></div>
      )}
    </div>
  );
}

// System › Backup: download the device's backup (settings and profiles in one .txt); restore a file after the page
// checked it with the device's rules, with a confirm, progress (s:status.restore) and the restart notice
import { useEffect, useRef } from "preact/hooks";
import { signal } from "@preact/signals";
import { device } from "../../store/app.ts";
import { Frame } from "../../rows/Frame.tsx";
import { confirm } from "../../shell/Confirm.tsx";
import { openSheet, closeSheet, SheetButtons } from "../../shell/Sheet.tsx";
import { download } from "../../shell/files.ts";
import { checkBackup, backupErrorText, BACKUP_MAX_BYTES } from "../../model/backup.ts";
import { busyText } from "../../texts/update.ts";
import type { Row } from "../../layout/types.ts";
import { Icon } from "../../art/icon.tsx";
import { SysGroup, Progress, MsgRow } from "./parts.tsx";

type Restore = { state: "idle" } | { state: "sending" } | { state: "applying"; pct: number } | { state: "ok" } | { state: "error"; text: string };
const restore = signal<Restore>({ state: "idle" });
const log = (m: string) => console.debug("[backup] " + m);

// The device's codes (api-device.cpp runRestore, backupErrorName)
const RESTORE_WORDS: Record<string, string> = {
  header: "not a GBSC backup", section: "a section is unknown or misplaced", syntax: "a line is not a setting", empty: "the backup is empty",
  read: "the file could not be read", space: "not enough space for the profiles", write: "a file could not be written (the device restarts)",
};

const today = () => { const d = new Date(); const p = (n: number) => String(n).padStart(2, "0"); return `${d.getFullYear()}-${p(d.getMonth() + 1)}-${p(d.getDate())}`; };

async function downloadBackup(): Promise<void> {
  const res = await device.client.get("/api/system/backup");
  log(`download: HTTP ${res.status}`);
  if (!res.ok) { device.toast(`Backup not downloaded (HTTP ${res.status})`, "error"); return; }
  const data = await res.arrayBuffer();
  download(data, `gbsc-backup-${today()}.txt`, "text/plain");
  if (data.byteLength > BACKUP_MAX_BYTES) {   // the device takes at most 640 KB back
    log(`download: ${data.byteLength} B, over the restore limit`);
    device.toast("This backup is over 640 KB: the device cannot restore it in one go. Delete some profiles or ADV presets and download it again.", "error");
  }
}

function refuse(text: string): void {
  openSheet({ title: "Not restored", icon: "error", body: () => (
    <><p>{text}</p><SheetButtons><button type="button" class="btn" onClick={closeSheet}>Close</button></SheetButtons></>
  ) });
}

async function restoreFile(file: File): Promise<void> {
  if (file.size > BACKUP_MAX_BYTES) { refuse(backupErrorText({ ok: false, error: "size", line: 0 })); return; }
  const c = checkBackup(await file.text());
  log(`file ${file.name}: ${c.ok ? `valid, ${c.profiles} profile(s)` : `${c.error} at line ${c.line}`}`);
  if (!c.ok) { refuse(backupErrorText(c)); return; }
  const adv = c.replacesAdv && c.adv ? `, ${c.adv} ADV preset${c.adv === 1 ? "" : "s"} or macro${c.adv === 1 ? "" : "s"}` : "";
  const what = c.replacesProfiles ? `Settings${adv} and ${c.profiles} profile${c.profiles === 1 ? "" : "s"} (they replace every profile on the device)` : `Settings${adv}`;
  if (!(await confirm({ icon: "settings_backup_restore", title: "Restore this backup?", ok: "Restore", body: `${what} come back from ${file.name}, then the device restarts.` }))) return;
  device.update.value = { ...device.update.value, restore: "" };
  restore.value = { state: "sending" };
  const form = new FormData();
  form.append("backup", file, file.name);
  let res: Response;
  try { res = await device.client.exclusive("/api/system/restore", form); } catch (e) {
    log(`restore: ${String(e)}`);
    restore.value = { state: "error", text: "The upload stopped: the device did not answer." };
    return;
  }
  log(`restore: HTTP ${res.status}`);
  if (res.status === 202 || res.ok) { if (restore.value.state === "sending") restore.value = { state: "applying", pct: 0 }; return; }
  restore.value = { state: "error", text: res.status === 503 ? busyText(res.headers.get("X-GBSC-Busy"), "restored")
    : res.status === 413 ? backupErrorText({ ok: false, error: "size", line: 0 }) : `Not restored (HTTP ${res.status}).` };
}

function StatusRow() {
  const s = restore.value;
  if (s.state === "sending" || s.state === "applying")
    return (
      <Frame row={{ kind: "live", label: "Restoring", icon: "settings_backup_restore" }} label="Restoring" sub="Keep the GBSC PRO on until it restarts." reason={null} wide noHelp>
        <Progress pct={s.state === "applying" ? s.pct : 0} label="Restoring" />
      </Frame>
    );
  if (s.state === "ok") return <MsgRow icon="check_circle" tone="good" text="Restored: the device restarts and the page reconnects by itself." />;
  if (s.state === "error") return <MsgRow icon="error" tone="bad" text={s.text} />;
  return null;
}

const ROWS: Record<string, Row> = {
  download: { kind: "action", label: "Download backup", icon: "download" },
  restore: { kind: "action", label: "Restore from a file", icon: "upload_file" },
};

export function BackupSection(_: { desk: boolean }) {
  const input = useRef<HTMLInputElement>(null);
  const raw = device.update.value.restore;
  useEffect(() => {   // s:status.restore: a percentage, ok, or error:<code>
    if (!raw || restore.value.state === "idle") return;
    if (raw === "ok") restore.value = { state: "ok" };
    else if (raw.startsWith("error:")) restore.value = { state: "error", text: `Not restored: ${RESTORE_WORDS[raw.slice(6)] ?? raw.slice(6)}.` };
    else if (/^\d+$/.test(raw)) restore.value = { state: "applying", pct: Number(raw) };
    log(`device: ${raw}`);
  }, [raw]);
  const busy = restore.value.state === "sending" || restore.value.state === "applying";
  return (
    <SysGroup title="Backup file">
      <Frame row={ROWS.download} label="Download backup" sub="Settings and profiles in one .txt file." reason={null} noHelp>
        <button type="button" class="btn" aria-label="Download backup" onClick={() => void downloadBackup()}><Icon name="download" />Download</button>
      </Frame>
      <Frame row={ROWS.restore} label="Restore from a file" sub="A .txt made by Download backup." reason={null} noHelp>
        <button type="button" class="btn" aria-label="Choose a file to restore" disabled={busy} onClick={() => input.current?.click()}><Icon name="upload_file" />Choose file</button>
      </Frame>
      <StatusRow />
      <input ref={input} type="file" accept=".txt,text/plain" hidden
             onChange={(e) => { const el = e.target as HTMLInputElement; const f = el.files?.[0]; el.value = ""; if (f) void restoreFile(f); }} />
    </SysGroup>
  );
}

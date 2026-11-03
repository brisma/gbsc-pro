// The Tauri side: commands of app/src/commands.rs, events, file dialogs and dropped files
import { invoke } from "@tauri-apps/api/core";
import { listen } from "@tauri-apps/api/event";
import { getCurrentWebview } from "@tauri-apps/api/webview";
import { documentDir } from "@tauri-apps/api/path";
import { open, save } from "@tauri-apps/plugin-dialog";
import { inFolder, type CandidatesRow, type DeviceRow, type FileRow } from "./state.ts";

export interface FlashResult { checked: boolean | null; usedStub: boolean; seconds: number }
export interface UnitRow { fw: string; board: string; advVersion: string | null; advUpdateAvailable: boolean; safeMode: boolean }
export interface ProfileRow { id: string; name: string; region: string }
export interface ProgressEvent { job: string; status: string | null; done: number | null; total: number | null; finished: string | null }

export const api = {
  devices: () => invoke<DeviceRow[]>("list_devices"),
  readFile: (path: string) => invoke<FileRow>("read_file", { path }),
  candidates: () => invoke<CandidatesRow>("candidates"),
  stubPresent: () => invoke<boolean>("stub_present"),
  flash: (port: string, path: string | null, erase: string, force: boolean) => invoke<FlashResult>("flash", { port, path, erase, force }),
  findUnits: () => invoke<string[]>("find_units"),
  unitInfo: (host: string) => invoke<UnitRow>("unit_info", { host }),
  update: (host: string, path: string) => invoke<string>("unit_update", { host, path }),
  updateAdv: (host: string) => invoke<string>("unit_update_adv", { host }),
  advVerify: (host: string, path: string) => invoke<boolean>("adv_verify", { host, path }),
  backup: (host: string, savePath: string) => invoke<void>("unit_backup", { host, savePath }),
  readText: (path: string) => invoke<string>("read_text", { path }),
  restore: (host: string, text: string) => invoke<void>("unit_restore", { host, text }),
  profiles: (host: string) => invoke<ProfileRow[]>("unit_profiles", { host }),
  profileExport: (host: string, id: string, savePath: string) => invoke<void>("unit_profile_export", { host, id, savePath }),
  profileImport: (host: string, path: string) => invoke<void>("unit_profile_import", { host, path }),
};

export const onDevices = (f: (rows: DeviceRow[]) => void) => listen<DeviceRow[]>("devices-changed", (e) => f(e.payload));
export const onProgress = (f: (p: ProgressEvent) => void) => listen<ProgressEvent>("progress", (e) => f(e.payload));
export const onDrop = (f: (path: string) => void) =>
  getCurrentWebview().onDragDropEvent((e) => { if (e.payload.type === "drop" && e.payload.paths.length) f(e.payload.paths[0]); });

let folder: Promise<string> | null = null, docs: Promise<string> | null = null;
/** Firmware dialogs start in the flasher's folder (where its firmware files are), not in the process folder
 *  (C:\Windows\System32 on Windows when started from another program). */
const startFolder = () => (folder ??= api.candidates().then((c) => c.dir).catch(() => ""));
/** Backups and profiles go to Documents: the flasher's folder may be /usr/bin (Linux package) or /Applications. */
const docsFolder = () => (docs ??= documentDir().catch(() => ""));

export async function pickFile(name: string, extensions: string[], where: "flasher" | "docs" = "flasher"): Promise<string | null> {
  const dir = await (where === "docs" ? docsFolder() : startFolder());
  const r = await open({ multiple: false, directory: false, filters: [{ name, extensions }], defaultPath: dir || undefined });
  return typeof r === "string" ? r : null;
}
/** A backup or profile file to write: named `name` in Documents. */
export const pickSave = async (name: string) => save({ defaultPath: inFolder(await docsFolder(), name) });
export const errText = (e: unknown) => (typeof e === "string" ? e : e instanceof Error ? e.message : JSON.stringify(e));

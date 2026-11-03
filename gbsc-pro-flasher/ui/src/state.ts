// The flasher's decisions, pure so the Node tests can run them: which flash is allowed, what needs a confirmation,
// which file is preselected. Backup checks and version order come from the web app (one source of the rules).
import { checkBackup, backupErrorText } from "../../../gbs-control/public/app/model/backup.ts";
import { compareVersions } from "../../../gbs-control/public/app/model/version.ts";

export type Kind = "esp" | "adv" | "advConsole" | "unknown";
export type EraseChoice = "none" | "user" | "wifi" | "all";

export interface DeviceRow { port: string; kind: Kind; label: string; refuse: string | null; confirm: string | null }
export interface FileRow { path: string; name: string; kind: Kind; version: string | null; bySizeOnly: boolean; advCrc: number | null; gzipped: boolean }
/** esp: the USB file (.bin first), espWifi: the WiFi file (.bin.gz first) of the same release. */
export interface CandidatesRow { esp: FileRow | null; espWifi: FileRow | null; adv: FileRow | null; dir: string; translocated: boolean; fromZip: boolean }
export interface LastFlash { kind: Kind; ok: boolean; file?: FileRow | null; port?: string; ports?: string[] }   // ports: the list when it ended

/** Flash only a file made for the device; an ESP8266 can also be erased without a file. */
export function flashEnabled(device: DeviceRow | null, file: FileRow | null, erase: EraseChoice): boolean {
  if (!device || device.refuse || (device.kind !== "esp" && device.kind !== "adv")) return false;
  if (!file) return device.kind === "esp" && erase !== "none";
  return file.kind === device.kind;
}

/** The confirmation text for a file without header or marker (factory or upstream images), or null. */
export function sizeOnlyWarning(file: FileRow): string | null {
  return file.bySizeOnly
    ? `${file.name} has no GBSC header or version marker: the flasher can only guess what it is from its size. Flash it only if you know it is the right image for this device.`
    : null;
}

/** True when the file is older than what the unit runs (asks before installing). */
export function isDowngrade(unitFw: string, fileVersion: string | null): boolean {
  return fileVersion !== null && compareVersions(fileVersion, unitFw) < 0;
}

/** Why a backup file cannot be restored (the device's own rules), or null when it can. */
export function restoreProblem(text: string): string | null {
  const c = checkBackup(text);
  return c.ok ? null : backupErrorText(c);
}

/** The file found next to the app for this device, or null. */
export function preselect(c: CandidatesRow | null, device: DeviceRow | null): FileRow | null {
  if (!c || !device) return null;
  return device.kind === "esp" ? c.esp : device.kind === "adv" ? c.adv : null;
}

/** After a USB flash of the ADV Controller the version is checked over WiFi (HCMGBoot returns no checksum). */
export function advCheckOffered(last: LastFlash | null): boolean {
  return !!last && last.ok && last.kind === "adv";
}

/** The file the post-flash WiFi check compares with: the one flashed, whatever the file step shows since. */
export function advCheckFile(last: LastFlash | null): FileRow | null {
  return advCheckOffered(last) && last!.file?.advCrc != null ? last!.file : null;
}

const usable = (d: DeviceRow) => !d.refuse && (d.kind === "esp" || d.kind === "adv");

/** The end step stays while the flashed port is there or nothing flashable replaced it (the ADV Controller restarts
 *  as its application for the WiFi check); another flashable device starts a new flash. */
/** The end step stays unless another flashable device is chosen by the user or newly plugged: a port that was
 *  already there when the flash ended (the GBSC's own USB cable during an ADV Controller flash) is no new flash. */
export function keepLast(last: LastFlash | null, selected: DeviceRow | null, byUser: boolean): boolean {
  if (!last || !selected || !usable(selected) || selected.port === last.port) return true;
  return !byUser && !!last.ports?.includes(selected.port);
}

/** The device selected after a list change: the user's choice while it is there, else a flashable one that needs
 *  no confirmation, else the first flashable one, else the first. */
export function defaultDevice(rows: DeviceRow[], selected: string | null): string | null {
  if (rows.some((d) => d.port === selected)) return selected;
  return (rows.find((d) => usable(d) && !d.confirm) ?? rows.find(usable) ?? rows[0])?.port ?? null;
}

export interface Question { title: string; body: string; ok: string }

/** The confirmations before a flash, in order: the device, a file without marker, an erase. */
export function flashQuestions(d: DeviceRow, f: FileRow | null, erase: EraseChoice): Question[] {
  const q: Question[] = [];
  if (d.confirm) q.push({ title: "Is this the ADV Controller?", body: d.confirm, ok: "Flash it" });
  const warn = f && sizeOnlyWarning(f);
  if (warn) q.push({ title: "Flash this file?", body: warn, ok: "Flash anyway" });
  if (erase !== "none" && d.kind === "esp") q.push({ title: f ? "Erase before writing?" : "Erase the GBSC?", body: eraseConfirm(erase, !!f), ok: "Erase" });
  return q;
}

const ERASE_LABEL: Record<EraseChoice, string> = {
  none: "Keep everything", user: "Erase settings and profiles", wifi: "Erase WiFi settings", all: "Erase everything",
};

/** The confirmation before an erase. Without a file the user area erase also takes the firmware (0x0..0x3FC000). */
export function eraseConfirm(choice: EraseChoice, hasFile: boolean): string {
  const what = ERASE_LABEL[choice];
  if (!hasFile && (choice === "user" || choice === "all"))
    return `${what}: the GBS-Control firmware is erased too, so the GBSC will not start until you flash it again. This cannot be undone.`;
  return `${what}: this cannot be undone.`;
}

export interface FlashEnd { kind: Kind; ok: boolean; checked: boolean | null; seconds: number; erasedOnly: boolean; error?: string | null; erase?: EraseChoice }

/** The end step's text: what happened (a failure with its reason) and what to do with the cable. */
export function endText(e: FlashEnd): string {
  if (!e.ok) return e.error ? `The write did not finish: ${e.error}` : "The write did not finish: see the message below, then try again.";
  if (e.kind === "adv") return "Written. Unplug the USB-C cable and power the GBSC normally, then check the version over WiFi.";
  const s = Math.round(e.seconds);
  if (e.erasedOnly) return e.erase === "user" || e.erase === "all"   // the firmware went with it
    ? `Erased in ${s} s. Flash a GBS-Control firmware now: the GBSC does not start without one.`
    : `Erased in ${s} s. The GBSC restarts.`;
  return e.checked ? `Written and verified in ${s} s. The GBSC restarts by itself.`
                   : `Written in ${s} s, not verified (slow mode). The GBSC restarts by itself.`;
}

/** A file dialog's start path: `name` in the flasher's folder (the folder with the firmware files), Windows or POSIX. */
export function inFolder(dir: string, name: string): string {
  if (!dir) return name;
  const sep = dir.includes("\\") ? "\\" : "/";
  return dir.endsWith(sep) ? dir + name : dir + sep + name;
}

/** Find on the network found nothing. Windows asks for the Firewall permission at the first search, and "Cancel"
 *  there blocks the GBSC's answers. */
export function notFoundHint(windows: boolean): string {
  const base = "No GBSC answered on gbscontrol.local. If it has just been switched on, wait a few seconds and search again; otherwise type its IP address (TV menu: System > WiFi).";
  return windows ? `${base} If the Windows Firewall asked about the flasher and you chose Cancel, allow it in Windows Security > Firewall > Allow an app.` : base;
}

/** A typed or pasted address: no spaces, no http(s):// and no path ("http://192.168.1.50/" gave http://http://...). */
export function cleanHost(h: string): string {
  return h.trim().replace(/^[a-z]+:\/\//i, "").replace(/\/.*$/, "");
}

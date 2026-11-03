// Backup file check before a restore: the rules of backupValidate (pro/api/api-backup.cpp) and of the device's line
// reader (lineAssemblerFeed: one trailing CR dropped, lines over 127 characters skipped), with the line at fault
export const BACKUP_MAX_BYTES = 655360;   // BACKUP_MAX_BYTES, the device's upload limit
const HEADER = "# gbsc-backup 1";
const SETTINGS_FILES = ["cfg/device.cfg", "cfg/current.cfg"];
const PROFILE_MAX = 36;
const LINE_MAX = 127;

export type BackupError = "header" | "section" | "syntax" | "empty" | "size";
export type BackupCheck = { ok: true; profiles: number; replacesProfiles: boolean; adv: number; replacesAdv: boolean } |
  { ok: false; error: BackupError; line: number };

type Kind = "none" | "settings" | "marker" | "raw" | "profile" | "advMarker" | "advPreset" | "advMacro";
const ADV_MAX_BYTES = { advPreset: 8192, advMacro: 2048 };   // adv-store.h

// ADV preset and macro texts (advYamlFeed): no control characters but tab, a non-empty name line, the size limit
class AdvText {
  bytes = 0; named = false; bad = false; big = false; max: number;
  constructor(max: number) { this.max = max; }
  feed(line: string): void {
    this.bytes += new TextEncoder().encode(line).length + 1;
    if (this.bytes > this.max) this.big = true;
    if (/[\x00-\x08\x0a-\x1f\x7f]/.test(line)) this.bad = true;
    const m = /^name:(.*)$/.exec(line);
    if (m) { let v = m[1].trim(); if (v.length >= 2 && (v[0] === "'" || v[0] === '"') && v.at(-1) === v[0]) v = v.slice(1, -1); this.named = v.length > 0; }
  }
  ok(): boolean { return !this.bad && !this.big && this.named; }
}

function sectionKind(line: string): Kind | null {
  if (!line.startsWith("[")) return null;
  if (line.length < 3 || !line.endsWith("]")) return "none";
  const body = line.slice(1, -1);
  if (body === "profiles") return "marker";
  if (body === "raw cfg/profiles.cfg") return "raw";
  if (body === "adv") return "advMarker";
  const a = /^adv (preset|macro) ([pm])(\d\d)$/.exec(body);
  if (a) return a[2] === (a[1] === "preset" ? "p" : "m") && Number(a[3]) < (a[1] === "preset" ? 16 : 32) ? (a[1] === "preset" ? "advPreset" : "advMacro") : "none";
  if (body.startsWith("file ")) return SETTINGS_FILES.includes(body.slice(5)) ? "settings" : "none";
  const m = /^profile p(\d\d)$/.exec(body);
  return m && Number(m[1]) < PROFILE_MAX ? "profile" : "none";
}

export function checkBackup(text: string): BackupCheck {
  if (new TextEncoder().encode(text).length > BACKUP_MAX_BYTES) return { ok: false, error: "size", line: 0 };
  let header = false, marker = false, advMarker = false, sections = 0, profiles = 0, adv = 0;
  let cur: Kind = "none", profileLine = 0, profileNamed = false;
  let advText: AdvText | null = null;
  const isAdv = (k: Kind) => k === "advPreset" || k === "advMacro";
  const close = (): boolean => {   // the section that ends: a profile needs its name, an ADV text its rules
    if (cur === "profile") { if (!profileNamed) return false; profiles++; }
    if (isAdv(cur)) { if (!advText!.ok()) return false; adv++; }
    return true;
  };
  const fail = (error: BackupError, line: number): BackupCheck => ({ ok: false, error, line });
  const lines = text.split("\n");
  if (lines[lines.length - 1] === "") lines.pop();
  for (let i = 0; i < lines.length; i++) {
    let line = lines[i];
    if (line.length > LINE_MAX) continue;   // the CR counts: the device keeps 127 characters of a line
    if (line.endsWith("\r")) line = line.slice(0, -1);
    const n = i + 1;
    if (!header) {
      if (line === "") continue;
      if (line !== HEADER) return fail("header", n);
      header = true;
      continue;
    }
    const kind = sectionKind(line);
    if (kind !== null) {
      if (kind === "none" || ((kind === "raw" || kind === "profile") && !marker) || (isAdv(kind) && !advMarker)) return fail("section", n);
      if (!close()) return fail("syntax", profileLine);
      if (kind === "marker") marker = true;
      if (kind === "advMarker") advMarker = true;
      advText = isAdv(kind) ? new AdvText(ADV_MAX_BYTES[kind as "advPreset" | "advMacro"]) : null;
      cur = kind; sections++; profileNamed = false; profileLine = n;
      continue;
    }
    if (isAdv(cur)) {   // a bad character points at its line, a text over the limit at its section
      advText!.feed(line);
      if (advText!.bad) return fail("syntax", n);
      if (advText!.big) return fail("syntax", profileLine);
      continue;
    }
    if (line === "" || line.startsWith("#")) continue;
    if (cur === "none" || cur === "marker" || cur === "advMarker" || !/^[a-z0-9._]+=/.test(line)) return fail("syntax", n);
    if (cur === "profile" && /^profile\.name=./.test(line)) profileNamed = true;
  }
  if (!close()) return fail("syntax", profileLine);
  if (!header) return fail("header", 0);
  if (!sections) return fail("empty", 0);
  return { ok: true, profiles, replacesProfiles: marker, adv, replacesAdv: advMarker };
}

export function backupErrorText(c: BackupCheck): string {
  if (c.ok) return "";
  switch (c.error) {
    case "header": return `Not a GBSC backup: the first line must be "${HEADER}"` + (c.line ? ` (line ${c.line})` : "");
    case "section": return `Unknown or misplaced section at line ${c.line}`;
    case "syntax": return `Line ${c.line}: settings are key=value lines, every profile needs a profile.name and every ADV preset or macro a name`;
    case "empty": return "The backup has no settings and no profiles";
    case "size": return "The file is bigger than 640 KB, the device's limit";
  }
}

// Port of settingsValidateRequest (pro/settings/settings-request.cpp) with settingsParse, settingsValid,
// settingsAvailable, settingsOptionAvailable and settingsStrValid (settings-registry.cpp). Checked against the C++
// results for the vectors in sim/vectors/ (test/unit/sim-contract.test.ts).
import type { Registry, RegEntry } from "./types.ts";
import { extraHostsValid } from "./guard.ts";

export interface ValidateResult {
  ok: number; errors: Record<string, string>; pairs: { key: string; value: number }[];
  str: { key: string; value: string } | null; errCount: number;
}

const MAX_PAIRS = 24;   // settingsPost kMax

export function findSetting(reg: Registry, key: string): RegEntry | undefined {
  return reg.settings.find((s) => s.key === key);
}

export function available(e: RegEntry, caps: number, devmode: boolean): boolean {
  if ((e.caps & caps) !== e.caps) return false;
  return devmode || !e.dev;
}

function parseUnsigned(text: string): number | null {
  let i = 0;
  while (text[i] === " " || text[i] === "\t") i++;
  if (!(text[i] >= "0" && text[i] <= "9")) return null;
  let v = 0;
  while (text[i] >= "0" && text[i] <= "9") { v = v * 10 + (text.charCodeAt(i) - 48); if (v > 65535) return null; i++; }
  while (text[i] === " " || text[i] === "\t" || text[i] === "\r") i++;
  return i === text.length ? v : null;
}

export function settingValid(e: RegEntry, v: number): boolean {
  if (v < (e.min ?? 0) || v > (e.max ?? 65535)) return false;
  if (e.type === "bool") return v <= 1;
  if (e.type === "enum") return e.options.some((o) => o.v === v);
  return true;
}

export function parseSetting(e: RegEntry, input: string): number | null {
  let text = input;
  while (text.startsWith(" ") || text.startsWith("\t")) text = text.slice(1);
  if (e.type === "bool") {
    if (text === "true" || text === "on") return 1;
    if (text === "false" || text === "off") return 0;
  }
  if (e.type === "enum") {
    const label = text.replace(/[ \r]+$/, "");
    const hit = e.options.find((o) => o.n.slice(0, 31) === label);
    if (hit) return hit.v;
  }
  const v = parseUnsigned(text);
  return v !== null && settingValid(e, v) ? v : null;
}

export function optionAvailable(e: RegEntry, v: number, caps: number): boolean {
  if (e.type !== "enum") return true;
  const o = e.options.find((x) => x.v === v);
  return !!o && (o.caps & caps) === o.caps;
}

export function strValid(e: RegEntry, value: string): boolean {
  if (value.length > (e.maxLen ?? 0)) return false;
  for (const ch of value) {
    const c = ch.charCodeAt(0);
    if (c < 0x20 || c > 0x7e || ch === '"' || ch === "\\") return false;
  }
  if (e.key === "system.extra_hosts") return extraHostsValid(value);
  return true;
}

export function validateRequest(reg: Registry, caps: number, devmode: boolean, keys: string[], values: string[]): ValidateResult {
  const r: ValidateResult = { ok: 0, errors: {}, pairs: [], str: null, errCount: 0 };
  const err = (k: string, reason: string) => { r.errors[k] = reason; r.errCount++; };
  for (let i = 0; i < keys.length; i++) {
    const k = keys[i], v = values[i];
    const e = findSetting(reg, k);
    if (!e) { err(k, "unknown"); continue; }
    if (e.type === "str") {
      if (e.ro) { err(k, "readonly"); continue; }
      if (r.str || !strValid(e, v)) { err(k, "invalid"); continue; }
      r.str = { key: k, value: v.slice(0, 63) };
      r.ok++;
      continue;
    }
    if (e.ro) { err(k, "readonly"); continue; }
    if (!available(e, caps, devmode)) { err(k, "unavailable"); continue; }
    const n = parseSetting(e, v);
    if (n === null) { err(k, "invalid"); continue; }
    if (!optionAvailable(e, n, caps)) { err(k, "unavailable"); continue; }
    if (r.pairs.length >= MAX_PAIRS) { err(k, "invalid"); continue; }
    r.pairs.push({ key: k, value: n });
    r.ok++;
  }
  return r;
}

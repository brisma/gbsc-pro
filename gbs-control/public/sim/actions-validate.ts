// Port of actionsPost's checks (pro/api/api-web.cpp) with the parse helpers and actionsValidateParams
// (pro/api/api-actions.cpp, shared/adv-link advRegReadReqUnpack). Checked against the C++ results for the vectors.
import type { ActionAll } from "./types.ts";

export interface ActionCheck { status: 200 | 400 | 404; parsed: number[] | null; text: string | null; entry?: ActionAll }

const DATA_MAX = 30;   // ActionRequest.data

function parseHexBytes(text: string, max = DATA_MAX): number[] | null {
  const out: number[] = [];
  let i = 0;
  while (i < text.length) {
    let v = 0, digits = 0;
    while (i < text.length && /[0-9a-fA-F]/.test(text[i])) { v = v * 16 + parseInt(text[i], 16); i++; if (++digits > 2) return null; }
    if (!digits || out.length >= max) return null;
    out.push(v);
    if (text[i] === ",") { i++; if (i >= text.length) return null; }
    else if (i < text.length) return null;
  }
  return out.length ? out : null;
}

function parseText(text: string): string | null {
  if (!text.length || text.length + 1 > DATA_MAX) return null;
  return /^[a-z0-9_]+$/.test(text) ? text : null;
}

function advRegReadValid(b: number[]): boolean {
  if (b.length !== 4) return false;
  const [dev, map, reg, count] = b;
  if (dev !== 0x42 && dev !== 0x56 && dev !== 0x84) return false;
  if (map && (dev !== 0x42 || (map !== 0x20 && map !== 0x40))) return false;
  return count >= 1 && count <= 32 && reg + count <= 0x100;
}

function paramsValid(a: ActionAll, data: number[]): boolean {
  if (!a.flags.includes("bytes")) return true;
  const p = a.params.find((x) => x.type === "bytes");
  const minLen = p?.len ?? 1, maxLen = p?.len ?? p?.max_len ?? DATA_MAX, maxValue = p?.max ?? 0xff;
  if (data.length < minLen || data.length > maxLen) return false;
  if (data.some((v) => v > maxValue)) return false;
  if (a.name === "dev.adv_read" && !advRegReadValid(data)) return false;
  return true;
}

export function validateAction(all: ActionAll[], caps: number, devmode: boolean, name: string, data: string | null): ActionCheck {
  const a = all.find((x) => x.name === name);
  if (!a || (a.caps & caps) !== a.caps || (!devmode && a.flags.includes("dev"))) return { status: 404, parsed: null, text: null };
  const has = data !== null;
  const f = (x: string) => a.flags.includes(x);
  let bytes: number[] | null = null, text: string | null = null;
  if (f("hex")) {
    bytes = has ? parseHexBytes(data as string) : null;
    if (!bytes || bytes.length % 3) return { status: 400, parsed: null, text: null, entry: a };
  }
  if (f("opt_bytes") && has) {
    bytes = parseHexBytes(data as string);
    if (!bytes) return { status: 400, parsed: null, text: null, entry: a };
  }
  if ((f("text") || f("opt_text")) && (has || f("text"))) {
    text = has ? parseText(data as string) : null;
    const words = a.params.find((x) => x.type === "text")?.values || [];
    if (!text || (words.length && !words.includes(text))) return { status: 400, parsed: null, text: null, entry: a };
  }
  if (f("bytes")) {
    bytes = has ? parseHexBytes(data as string) : null;
    if (!bytes) return { status: 400, parsed: null, text: null, entry: a };
  }
  if (!paramsValid(a, bytes || [])) return { status: 400, parsed: null, text: null, entry: a };
  return { status: 200, parsed: bytes && bytes.length ? bytes : null, text, entry: a };
}

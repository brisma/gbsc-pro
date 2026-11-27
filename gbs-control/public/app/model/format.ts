// Row values as the TV menu prints them (pro/ui/menu-model.cpp menuFormatSettingValue, formatLive)
import { registry, type MenuNode } from "./menu.ts";

export const onOff = (on: boolean) => (on ? "On" : "Off");

// A "live_value" quantity (Sync limit): the device also picks it by itself. While the row is n/a (Adaptive Sync not
// Manual) or the setting was never set by hand (0) the row shows the value in use; the first step starts from it
export const LIVE_VALUE_MIN = 40, LIVE_VALUE_START = 100;
export const isLiveValue = (n: MenuNode | undefined) => !!n?.flags.includes("live_value");
export function stepLiveValue(cur: number, dir: number, live: unknown, max: number): number {
  const now = Number(live);
  const base = cur || (now >= LIVE_VALUE_MIN && now <= max ? now : LIVE_VALUE_START);
  return Math.max(LIVE_VALUE_MIN, Math.min(max, base + dir));
}

// live: the /api/live value of a live_value node; na: the row is greyed
export function formatSetting(n: MenuNode, v: number | string | undefined, live?: unknown, na?: boolean): string {
  if (v === undefined) return "";
  const num = Number(v);
  if (isLiveValue(n) && (num === 0 || na)) return Number(live) > 0 ? String(Number(live)) : "-";
  let out: string;
  const list = n.options?.length ? n.options : undefined;
  const e = n.setting ? registry.entry(n.setting) : undefined;
  if (list) out = list.find((o) => o.v === num)?.n ?? "-";
  else if (e?.type === "bool") out = onOff(num !== 0);
  else if (e?.options?.length) out = e.options.find((o) => o.v === num)?.n ?? String(v);
  else out = String(v);
  return n.flags.includes("percent") ? out + "%" : out;
}

const SDRAM = ["108MHz", "81MHz", "FB", "162MHz", "144MHz", "185MHz", "216MHz", "129MHz"];
// v: the /api/live value (pairs as [h, v])
export function formatLive(lv: string, v: unknown): string {
  if (v === undefined || v === null) return "";
  const n = Number(v);
  switch (lv) {
    case "adc_gain": case "htotal": case "pll_div": case "sog": return String(n);
    case "debug_view": case "freeze": case "if_offset": case "syncwatcher": case "osd_always_on": case "osd_test":
    case "print_info": case "ir_log": case "ota": return onOff(n !== 0);
    case "position": case "size": case "borders": case "border_pos": return Array.isArray(v) ? `${v[0]}/${v[1]}` : "";
    case "sdram": return n >= 0 && n < 8 ? SDRAM[n] : "-";
    case "osr": return `${n}x`;
    case "adc_filter": return onOff(n > 0);
    case "sync_polarity": return n ? "Inverted" : "Normal";
    case "out_rate": return n >= 565 && n <= 645 ? "60Hz" : n >= 465 && n <= 545 ? "50Hz" : "-";
    default: return "";
  }
}

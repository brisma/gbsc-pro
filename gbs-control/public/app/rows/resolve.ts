// A row's firmware data and state: which menu node, setting or action it points at, whether it shows in this mode,
// board and input (hidden by cable, greyed with the TV reason by another setting)
import actionsJson from "../../gen/actions-all.json" with { type: "json" };
import { nodeById, registry, type MenuNode } from "../model/menu.ts";
import { visibleInTree, disabledReason, CAP, type PredState } from "../model/predicates.ts";
import type { Row, Group, Section, Cap, Ref } from "../layout/types.ts";

interface ActionInfo { name: string; caps: number; flags: string[] }
const ACTIONS = new Map((actionsJson as unknown as ActionInfo[]).map((a) => [a.name, a]));
export const LIVE_NAMES = ["adc_gain", "position", "size", "borders", "htotal", "pll_div", "sdram", "osr", "adc_filter", "debug_view",
  "freeze", "if_offset", "sog", "out_rate", "syncwatcher", "osd_always_on", "wifi_rssi", "osd_test", "print_info", "ir_log",
  "sync_polarity", "ota", "border_pos"];
// text:<source> rows: fields of /api/info and /api/wifi the pages print
export const TEXT_SOURCES = ["wifi.mode", "wifi.ssid", "wifi.ip", "wifi.hostname", "wifi.ap_password", "wifi.extra_hosts", "info.fw",
  "info.adv_controller", "info.update.channel", "info.update.last", "info.update.offer", "info.update.manual", "info.signal",
  "info.input", "profile.active"];

export function splitRef(ref: Ref): [string, string] {
  const i = ref.indexOf(":");
  return [ref.slice(0, i), ref.slice(i + 1)];
}

export function refExists(ref: string): boolean {
  const [kind, id] = splitRef(ref as Ref);
  switch (kind) {
    case "menu": return !!nodeById(id);
    case "setting": return !!registry.entry(id);
    case "action": return ACTIONS.has(id);
    case "live": return LIVE_NAMES.includes(id);
    case "text": return TEXT_SOURCES.includes(id);
    default: return false;
  }
}

export const rowNode = (r: Row): MenuNode | undefined => (r.ref?.startsWith("menu:") ? nodeById(r.ref.slice(5)) : undefined);
export const rowSetting = (r: Row): string | undefined =>
  r.ref?.startsWith("setting:") ? r.ref.slice(8) : rowNode(r)?.setting;
export const rowAction = (r: Row): string | undefined =>
  r.ref?.startsWith("action:") ? r.ref.slice(7) : rowNode(r)?.action;
export const actionInfo = (name: string | undefined) => (name ? ACTIONS.get(name) : undefined);

export function capOk(cap: Cap | undefined, caps: number): boolean {
  if (cap === "adv") return (caps & CAP.ADV) !== 0;
  if (cap === "osd") return (caps & CAP.OSD) !== 0;
  if (cap === "std") return (caps & CAP.ORIGINAL) !== 0;
  return true;
}

export interface RowState { visible: boolean; reason: string | null }

export function rowState(r: Row, s: PredState, g?: Group, sec?: Section): RowState {
  const hidden = { visible: false, reason: null };
  if ((r.advanced || g?.advanced) && s.menuMode === 0) return hidden;
  if (!capOk(r.cap, s.caps) || !capOk(g?.cap, s.caps) || !capOk(sec?.cap, s.caps)) return hidden;
  if (r.showIf && !r.showIf(s)) return hidden;
  const node = rowNode(r);
  if (node) return visibleInTree(node, s) ? { visible: true, reason: disabledReason(node, s) } : hidden;
  if (r.ref?.startsWith("setting:")) {
    const e = registry.entry(r.ref.slice(8));
    if (!e || ((e.caps ?? 0) & s.caps) !== (e.caps ?? 0) || (e.dev && !s.devmode)) return hidden;
  }
  if (r.ref?.startsWith("action:")) {
    const a = ACTIONS.get(r.ref.slice(7));
    if (!a || (a.caps & s.caps) !== a.caps || (a.flags.includes("dev") && !s.devmode)) return hidden;
  }
  return { visible: true, reason: null };
}

export function rowLabel(r: Row): string {
  return r.label ?? rowNode(r)?.label ?? (r.ref ? splitRef(r.ref)[1] : "");
}

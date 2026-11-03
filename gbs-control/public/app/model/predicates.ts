// Ports of predVisible / menuVisible / menuEnabled (pro/ui/menu-model.cpp:22-105), checked against the firmware's
// truth table (gen/predicates.json, test app-predicates). State.menuMode is the browser's Easy/Advanced.
import actionsJson from "../../gen/actions-all.json" with { type: "json" };
import { menu, registry, parentOf, type MenuNode, type Mode } from "./menu.ts";

export const CAP = { ADV: 1, OSD: 2, AUDIO: 4, IR: 8, OLED: 16, ENCODER: 32, LED: 64, ORIGINAL: 128 } as const;
// The exporter's boards (test_web_export kBoards): a Pro as before detection, the standard board bare
export const BOARD_CAPS = { pro: CAP.ADV | CAP.OSD | CAP.AUDIO | CAP.IR | CAP.OLED | CAP.ENCODER, standard: CAP.ORIGINAL | CAP.LED };

// InputType (pro/input/input-types.h)
export const IN = { RGBS: 1, RGSB: 2, VGA: 3, YUV: 4, SV: 5, AV: 6 } as const;

export interface PredSignal {
  noSignal: boolean; interlaced: boolean; scanlinesOk: boolean; advNtsc: boolean; updateAvailable: boolean;
  updateManual: boolean; advControllerUpdate: boolean; presetId: number; srcHz10: number;
}
export interface PredState {
  caps: number;
  input: number;          // the hardware input (Auto: the one found, the last found while searching), as MenuBackend::activeInput
  menuMode: number;       // 0 Easy, 1 Advanced: the browser mode
  devmode: boolean;
  profileActive: boolean;
  wifiAp: boolean;
  signal: PredSignal;
  values: Record<string, number | string>;
}

interface ActionInfo { name: string; caps: number; flags: string[] }
const actionByName = new Map((actionsJson as unknown as ActionInfo[]).map((a) => [a.name, a]));

const num = (s: PredState, k: string) => Number(s.values[k] ?? 0);
const decoder = (i: number) => i === IN.SV || i === IN.AV;
const hasCaps = (need: number, caps: number) => (need & caps) === need;

function predVisible(pred: string, s: PredState): boolean {
  const i = s.input;
  switch (pred) {
    case "devmode": return s.devmode && s.menuMode === 1;
    case "svav": case "i2p_on": return decoder(i);
    case "sv": case "sv_wy": return i === IN.SV;
    case "av": return i === IN.AV;
    case "av_ntsc": return i === IN.AV && s.signal.advNtsc;
    case "av_pal": return i === IN.AV && !s.signal.advNtsc;
    case "rgb_sync": return (s.caps & CAP.ADV) !== 0 && (i === IN.RGBS || i === IN.RGSB);
    case "rgbs": return (s.caps & CAP.ADV) !== 0;
    case "profile_active": return s.profileActive;
    case "update_available": return s.signal.updateAvailable;
    case "update_manual": return s.signal.updateManual;
    case "adv_controller_update": return s.signal.advControllerUpdate;
    case "vga": return i === IN.VGA;
    case "sog": return i === IN.RGSB || i === IN.YUV;
    case "tv_osd": return (s.caps & CAP.OSD) === 0 && !s.signal.noSignal && (s.signal.presetId & 0x0f) !== 6 && s.signal.presetId < 0x21;
    case "wifi_ap": return s.wifiAp;
    default: return true;
  }
}

function actionAvailable(name: string | undefined, s: PredState): boolean {
  const a = name ? actionByName.get(name) : undefined;
  if (!a) return false;
  return hasCaps(a.caps, s.caps) && (s.devmode || !a.flags.includes("dev"));
}

export function visible(n: MenuNode, s: PredState): boolean {
  if (n.flags.includes("advanced") && s.menuMode === 0) return false;
  if (!predVisible(n.pred, s)) return false;
  switch (n.type) {
    case "setting": {
      const e = registry.entry(n.setting ?? "");
      return !!e && hasCaps(e.caps ?? 0, s.caps) && (s.devmode || !e.dev);
    }
    case "action": case "stepper":
      return n.flags.includes("fn") || actionAvailable(n.action, s);
    case "adjust2d":
      return actionAvailable(menu.adjust[n.adjust ?? ""]?.left, s);
    case "sub":
      if (n.flags.includes("fn")) return true;
      return (n.children ?? []).some((k) => k.type !== "header" && visible(k, s));
    default:
      return true;
  }
}

// Visible where the TV shows it: the node and every sub above it (a decoder row needs the Decoder page)
export function visibleInTree(n: MenuNode, s: PredState): boolean {
  if (!visible(n, s)) return false;
  for (let p = parentOf(n.id); p && p.id !== "" && p.id !== "quick"; p = parentOf(p.id)) if (!visible(p, s)) return false;
  return true;
}

// The TV's reason text when the row is shown but cannot be used, else null
export function disabledReason(n: MenuNode, s: PredState): string | null {
  const r = menu.reasons;
  switch (n.pred) {
    case "sv_wy": return num(s, "adv.filter_wy_override") !== 1 ? r.wy_manual : null;
    case "rgbs": return s.input !== IN.RGBS ? r.rgbs : null;
    case "ace_on": return num(s, "adv.ace") !== 1 ? r.ace : null;
    case "scanlines_ok":
      if (s.signal.scanlinesOk) return null;
      return s.signal.interlaced && num(s, "picture.deinterlace") === 0 ? r.needs_bob : r.scanlines;
    case "i2p_on": return num(s, "adv.i2p") !== 1 ? r["2x_first"] : null;
    case "deint_ok": return decoder(s.input) && num(s, "adv.i2p") === 1 ? r["2x_on"] : null;
    case "ftl_ok":
      return num(s, "output.force_60hz") === 1 && !s.signal.noSignal && s.signal.srcHz10 > 0 && s.signal.srcHz10 < 550 ? r.force60 : null;
    case "ftl_on": return num(s, "output.frame_lock") !== 1 ? r.ftl_off : null;
    default: return null;
  }
}

export interface StoreView {
  values: Record<string, number | string>;
  info: any | null;
  wifiMode: string;     // /api/wifi mode ("AP" | "STA" | "")
  mode: Mode;           // the browser's Easy/Advanced
  profileActive: boolean;
}

// The predicates' state from what the page knows: settings, /api/info (input, signal, update offers), /api/wifi
export function stateFromStore(v: StoreView): PredState {
  const i = v.info ?? {};
  const g = i.signal ?? {};
  return {
    caps: typeof i.caps === "number" ? i.caps : BOARD_CAPS.pro,
    // while Auto searches the hardware walks the paths: the last found input keeps the page still
    input: Number((i.input?.state === "searching" ? i.input?.last : i.input?.active) ?? i.input?.active ?? v.values["input.source"] ?? 0),
    menuMode: v.mode === "advanced" ? 1 : 0,
    devmode: Number(v.values["system.devmode"] ?? 0) === 1,
    profileActive: v.profileActive,
    wifiAp: v.wifiMode === "AP",
    values: v.values,
    signal: {
      noSignal: v.info ? !!g.none : true,
      interlaced: !!g.interlaced,
      scanlinesOk: !!g.scanlines_ok,
      advNtsc: !!g.adv_ntsc,
      updateAvailable: !!i.update?.offer?.version,
      updateManual: !!i.update?.manual?.version,
      advControllerUpdate: !!i.adv_controller?.update_available,
      presetId: Number(g.preset ?? 0),
      srcHz10: Number(g.src_hz10 ?? 0),
    },
  };
}

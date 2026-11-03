// Quick pins: the rows of the Quick page, per browser (gbsc.quickPins, menu ids). Default = the
// menu's Quick items in order; Edit adds rows from the Settings sections, removes and reorders them.
import { menu, nodeById, registry, type MenuNode } from "../../model/menu.ts";
import { bindingFor } from "../../layout/bindings.ts";
import type { Row } from "../../layout/types.ts";

const SETTINGS_TOPS = ["input/", "output/", "picture/", "screen/"];

export function defaultPins(): string[] {
  return (menu.quick.children ?? []).map((n) => n.id).filter((id) => bindingFor(id)?.web !== "excluded");
}

// A node the web can draw as a Quick row
function rowable(id: string): boolean {
  const n = nodeById(id);
  return !!n && bindingFor(id)?.web !== "excluded" && (id.startsWith("quick/") || !!rowForNode(n));
}

export function cleanPins(raw: unknown): string[] {
  if (!Array.isArray(raw)) return defaultPins();
  const out: string[] = [];
  for (const id of raw) if (typeof id === "string" && !out.includes(id) && rowable(id)) out.push(id);
  return out;
}

export const addPin = (p: string[], id: string) => (p.includes(id) ? p : [...p, id]);
export const removePin = (p: string[], id: string) => p.filter((x) => x !== id);
export function movePin(p: string[], id: string, dir: -1 | 1): string[] {
  const i = p.indexOf(id), j = i + dir;
  if (i < 0 || j < 0 || j >= p.length) return p;
  const out = [...p];
  [out[i], out[j]] = [out[j], out[i]];
  return out;
}

const ICONS: Record<string, string> = { "input.source": "cable", "adv.i2p": "filter_2", resolution: "hd", filter: "photo_filter", scanlines: "gradient" };   // @icons
const DEF = { toggle: "toggle_on", value: "tune", pad: "open_with" };   // @icons

// The row a pinned node draws: settings by type, choices as cards or the resolution picker, pads, steppers
export function rowForNode(n: MenuNode): Row | null {
  const ref = `menu:${n.id}` as const;
  if (n.type === "setting" && n.setting) {
    const e = registry.entry(n.setting);
    if (!e || e.type === "str" || e.ro) return null;
    if (n.setting === "input.source") return { kind: "picker", ref, icon: "cable", look: "source" };
    if (e.type === "bool") return { kind: "toggle", ref, icon: ICONS[n.setting] ?? DEF.toggle };
    if (e.type === "enum" || n.options?.length) return { kind: "inline", ref, icon: ICONS[n.setting] ?? DEF.value };
    return { kind: "stepper", ref, icon: DEF.value };
  }
  if (n.type === "choice" && n.choice) {
    if (n.choice === "resolution") return { kind: "picker", ref, icon: "hd", look: "resolution" };
    return { kind: "cards", ref, icon: ICONS[n.choice] ?? DEF.value, look: n.choice as "filter" | "scanlines" };
  }
  if (n.type === "adjust2d") return { kind: "pad", ref, icon: DEF.pad };
  if (n.type === "stepper") return { kind: "stepper", ref, icon: DEF.value };
  return null;
}

// Offered by "Add from all settings": settings, choices and pads of the four Settings sections
export function pinnable(n: MenuNode): boolean {
  return SETTINGS_TOPS.some((t) => n.id.startsWith(t)) && ["setting", "choice", "adjust2d"].includes(n.type) && !!rowForNode(n);
}

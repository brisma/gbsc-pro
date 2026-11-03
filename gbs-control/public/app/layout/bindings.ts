// Where every node of the firmware menu (gen/menu.json) lives in the web app:
// a row of a section, a whole section or group, one of the pages with their own layout (WiFi, Update, Profiles,
// Status), the mode switch, the Scanlines cards, the remote; or excluded with the reason. Header nodes are titles.
import { allNodes, type MenuNode } from "../model/menu.ts";

export type Web = "row" | "section" | "group" | "mode-switch" | "scanline-cards" | "remote" | "wifi-page" | "update-page" |
  "profiles-page" | "status-page" | "excluded";
export interface Binding { id: string; web: Web; reason?: string; label?: string }

// By id (exact) or by id prefix ("system/update/" covers the subtree)
const EXACT: Record<string, Omit<Binding, "id">> = {
  "input": { web: "section" }, "output": { web: "section" }, "picture": { web: "section" }, "screen": { web: "section" },
  "profiles": { web: "profiles-page" }, "system": { web: "section", label: "System page" },
  "system/sound": { web: "group", label: "System › Device" }, "system/osd-settings": { web: "group", label: "System › TV and OLED menu" },
  "developer": { web: "section", label: "Developer page" },
  "input/auto-search": { web: "group", label: "Input: In Auto switches per cable (Advanced)" },
  "input/sampling": { web: "group" }, "input/decoder": { web: "group" }, "input/decoder/adaptive-contrast": { web: "group" },
  "developer/sync": { web: "section" }, "developer/clock-adc": { web: "section" }, "developer/debug": { web: "section" },
  "developer/serial-log": { web: "section" },
  "system/signal-info": { web: "status-page", label: "Status › Signal" },
  "system/developer-menu": { web: "mode-switch", label: "Dev in the header switch (web and TV)" },
  "picture/scanline-strength": { web: "scanline-cards", label: "Advanced Scanlines cards (Off + 10…60)" },
  "quick/tv-osd": { web: "excluded", reason: "no web action: the TV5725 icons show on the TV itself (standard board Quick menu)" },
};
const PREFIX: [string, Omit<Binding, "id">][] = [
  ["profiles/", { web: "profiles-page" }],
  ["system/wifi/", { web: "wifi-page" }], ["system/wifi", { web: "wifi-page" }],
  ["system/update/", { web: "update-page" }], ["system/update", { web: "update-page" }],
];

export function bindingFor(id: string): Binding | undefined {
  const n = allNodes().find((x) => x.id === id);
  if (!n || n.type === "header" || id === "") return undefined;
  if (EXACT[id]) return { id, ...EXACT[id] };
  for (const [p, b] of PREFIX) if (id === p || id.startsWith(p)) return { id, ...b };
  return { id, web: "row" };
}

export const BINDINGS: Binding[] = allNodes().filter((n: MenuNode) => n.id !== "" && n.type !== "header").map((n) => bindingFor(n.id)!);

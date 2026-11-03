// The firmware menu tree (gen/menu.json, printed from pro/ui/menu-tree.cpp by the native exporter): labels, order,
// Easy/Advanced flags, option lists, choices and adjust actions. The page hardcodes none of these.
import menuJson from "../../gen/menu.json" with { type: "json" };
import registryJson from "../../gen/registry.json" with { type: "json" };
import { indexRegistry, type Registry, type RegOption } from "./registry.ts";

export interface MenuNode {
  id: string;
  type: string;   // sub, setting, choice, stepper, action, toggle_fn, live, adjust2d, header
  label: string;
  help: string;
  flags: string[];
  pred: string;
  setting?: string;
  options?: { n: string; v: number }[];
  step?: number;
  action?: string;
  action_inc?: string;
  fn?: string;
  live?: string;
  text?: string;
  choice?: string;
  adjust?: string;
  children?: MenuNode[];
}
export interface ChoiceWrite { key: string; value: number }
export interface ChoiceOpt { label: string; writes: ChoiceWrite[]; flags: string[] }
export interface MenuData {
  version: number;
  root: MenuNode;
  quick: MenuNode;
  choices: Record<string, ChoiceOpt[]>;
  adjust: Record<string, { left: string; right: string; up: string; down: string }>;
  reasons: Record<string, string>;
}

export const menu = menuJson as unknown as MenuData;
export const registry = indexRegistry(registryJson as unknown as Registry);
export type Mode = "easy" | "advanced";

// Bool settings the page names by what they pick instead of On and Off (the TV still prints On/Off)
const WEB_OPTIONS: Record<string, { n: string; v: number }[]> = {
  "output/analog-out": [{ n: "RGB", v: 0 }, { n: "YPbPr", v: 1 }],
};

const byId = new Map<string, MenuNode>();
const parents = new Map<string, MenuNode>();
const order: MenuNode[] = [];
(function index(n: MenuNode, parent: MenuNode | null) {
  if (WEB_OPTIONS[n.id]) n.options = WEB_OPTIONS[n.id];
  byId.set(n.id, n);
  order.push(n);
  if (parent) parents.set(n.id, parent);
  for (const c of n.children ?? []) index(c, n);
})(menu.root, null);
for (const c of menu.quick.children ?? []) { byId.set(c.id, c); order.push(c); parents.set(c.id, menu.quick); }

export const nodeById = (id: string): MenuNode | undefined => byId.get(id);
export const parentOf = (id: string): MenuNode | undefined => parents.get(id);
export const labelFor = (id: string): string => byId.get(id)?.label ?? id;
export const allNodes = (): MenuNode[] => order;
export const isAdvanced = (n: MenuNode) => n.flags.includes("advanced");

export interface OptionCtx { mode: Mode; caps: number; current: number | undefined; profileActive: boolean }

// The values a setting row offers, as menuStepFrom steps them: the node's own list, else the registry options
// without missing hardware, without Custom (output.preset 2) when no profile is active, and without Advanced-only
// options in Easy unless one is the current value
export function optionsFor(n: MenuNode, c: OptionCtx): { n: string; v: number }[] {
  if (n.options?.length) return n.options;
  const e = n.setting ? registry.entry(n.setting) : undefined;
  const list: RegOption[] = e?.options ?? [];
  const preset = n.setting === "output.preset";
  return list
    .filter((o) => ((o.caps ?? 0) & c.caps) === (o.caps ?? 0))
    .filter((o) => !(preset && o.v === 2 && !c.profileActive))
    .filter((o) => !(c.mode === "easy" && o.advanced && o.v !== c.current))
    .map((o) => ({ n: o.n, v: o.v }));
}

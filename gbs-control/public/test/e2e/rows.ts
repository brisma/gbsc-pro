// The rows every-row.spec.ts checks, from layout/pages.ts (a module, so other specs can read the list without
// importing a test file)
import { PAGES } from "../../app/layout/pages.ts";
import type { Row, Group, Section, PageId } from "../../app/layout/types.ts";
import { rowNode, rowSetting, rowAction } from "../../app/rows/resolve.ts";
import { menu } from "../../app/model/menu.ts";

export const FN_ACTIONS: Record<string, string> = { picture_reset: "picture.reset", geometry_reset: "screen.reset", decoder_reset: "adv.decoder_reset", termination: "input.termination" };

export interface Item { page: PageId; section: Section; group: Group; row: Row; title: string }

function itemTitle(r: Row): string | null {
  const n = rowNode(r);
  if (r.kind === "note" || r.kind === "link" || r.kind === "live" || r.kind === "secret") return null;   // secret: dev.spec.ts
  if (n?.type === "choice") return `every-row choice:${n.choice} advanced`;
  if (n?.type === "toggle_fn") return `every-row action:${FN_ACTIONS[n.fn ?? ""]}`;
  if (n?.type === "adjust2d") return `every-row action:${menu.adjust[n.adjust ?? ""].up}`;
  if (n?.type === "stepper") return `every-row action:${n.action_inc}`;
  if (r.kind === "action" || n?.type === "action") return `every-row action:${(n?.fn ? FN_ACTIONS[n.fn] : undefined) ?? rowAction(r)}`;
  const key = rowSetting(r);
  return key ? `every-row setting:${key} advanced` : null;
}

export const ITEMS: Item[] = [];
for (const p of PAGES) for (const s of p.sections) for (const g of s.groups ?? []) for (const r of g.rows) {
  const t = itemTitle(r);
  if (t && !ITEMS.some((i) => i.title === t)) ITEMS.push({ page: p.id, section: s, group: g, row: r, title: t });
}


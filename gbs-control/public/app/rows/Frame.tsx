// One row: icon, title with the "?" help, one line of text (the first sentence of the help, or the TV reason when
// greyed), the control; a panel may follow it. The DOM is updated in place (no entrance animation on a value change).
import type { ComponentChildren } from "preact";
import { Icon } from "../art/icon.tsx";
import { openHelp } from "../shell/HelpSheet.tsx";
import { helpFor } from "../texts/help.ts";
import { T } from "../texts/ui.ts";
import { nodeById, allNodes, isAdvanced, parentOf } from "../model/menu.ts";
import { pageById } from "../layout/pages.ts";
import { route } from "../shell/router.ts";
import type { Row } from "../layout/types.ts";
import { rowNode } from "./resolve.ts";
import { HelpPicture } from "./looks.tsx";

export interface FrameProps {
  row: Row; label: string; reason: string | null;
  two?: boolean;      // the control sits under the text, across the whole row (strips, progress bars)
  wide?: boolean;     // a wide control: beside the text on PC, on a line of its own on the phone (segments, pads)
  list?: boolean;     // a list row (networks, profiles): on the phone the control stays centred on the two lines
  lnk?: boolean; sub?: string; cls?: string;
  onRow?: () => void; open?: boolean; children?: ComponentChildren; panel?: ComponentChildren; noHelp?: boolean;
}

// The main-tree node a Quick row repeats (same setting or choice), for its help text and "In Settings › X"
export function homeOf(id: string): string | undefined {
  if (!id.startsWith("quick/")) return id;
  const q = nodeById(id);
  return allNodes().find((n) => !n.id.startsWith("quick/") && n.id !== q?.id && ((q?.setting && n.setting === q.setting) || (q?.choice && n.choice === q.choice)))?.id;
}

// The web section a setting lives in ("Scaler" for Smooth scrolling), for "In Settings › X"
function webSection(home: string): string {
  for (const sec of pageById("settings")?.sections ?? [])
    for (const g of sec.groups ?? []) if (g.rows.some((r) => r.ref === `menu:${home}` || r.ref?.startsWith(`menu:${home}~`))) return sec.title;
  let n = nodeById(home), p = n ? parentOf(n.id) : undefined;
  while (p && p.id !== "" && parentOf(p.id)?.id !== "") { n = p; p = parentOf(p.id); }
  return p && p.id !== "" ? p.label : n?.label ?? "";
}

// The full help of a row and its first sentence (the one line shown under the title)
function helpTexts(row: Row): { title?: string; full: string; first: string } {
  const node = rowNode(row);
  const h = helpFor(row.help ?? node?.id ?? "", node ? homeOf(node.id) : undefined);
  const full = h?.[1] ?? node?.help ?? "";
  return { title: h?.[0], full, first: full.match(/^.*?[.!?](?=\s|$)/)?.[0] ?? full };
}

function showHelp(row: Row, label: string): void {
  const node = rowNode(row);
  const home = node ? homeOf(node.id) : undefined;
  const h = helpTexts(row);
  const homeNode = home ? nodeById(home) : undefined;
  const away = homeNode && route.value.page !== "settings" && !home!.startsWith("system") && !home!.startsWith("developer");
  openHelp({
    title: h.title ?? label, icon: row.icon, text: h.full,
    picture: row.look === "scanlines" || row.look === "filter" ? () => <HelpPicture look={row.look!} /> : undefined,
    where: away ? T.inSettings(`Settings › ${webSection(home!)}`, isAdvanced(homeNode!)) : undefined,
  });
}

export function hasHelp(row: Row): boolean {
  if (row.kind === "action" || row.kind === "note") return false;
  const node = rowNode(row);
  return !!(helpFor(row.help ?? node?.id ?? "", node ? homeOf(node.id) : undefined) || node?.help);
}

export function Frame(p: FrameProps) {
  const cls = `rw${p.two ? " two" : ""}${p.wide ? " wide" : ""}${p.list ? " list" : ""}${p.reason ? " dis" : ""}${p.lnk ? " lnk" : ""}${p.cls ? " " + p.cls : ""}`;
  const line = p.sub ?? (p.row.kind === "action" || p.row.kind === "note" ? "" : helpTexts(p.row).first);
  return (
    <>
      {/* a greyed row is an inactive group (WCAG 1.4.3 exempts its text from the contrast minimum; axe reads this) */}
      <div class={cls} data-row={`${p.row.kind}:${p.row.ref ?? p.label}`} onClick={p.onRow}
           role={p.reason ? "group" : undefined} aria-disabled={p.reason ? "true" : undefined} aria-label={p.reason ? p.label : undefined}>
        <span class={p.row.tint ? `ic t-${p.row.tint}` : "ic"}><Icon name={p.row.icon} fill={!!p.row.tint} /></span>
        <span class="lb">
          <span>{p.label}</span>
          {!p.noHelp && hasHelp(p.row) ? <button type="button" class="q" aria-label={T.help(p.label)} onClick={(e) => { e.stopPropagation(); showHelp(p.row, p.label); }}>?</button> : null}
        </span>
        {p.reason ? <span class="ds why">{p.reason}</span> : line ? <span class="ds">{line}</span> : null}
        <div class="c">{p.children}</div>
      </div>
      {p.panel}
    </>
  );
}

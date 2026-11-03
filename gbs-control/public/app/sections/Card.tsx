// The blocks every page is made of: a card with a header band (tile, title, one line that says what the card is for,
// its actions on the right) and, inside it, bands that open the groups of rows
import type { ComponentChildren } from "preact";
import { Icon } from "../art/icon.tsx";

export interface CardProps {
  icon: string; title: string; sub?: string;
  actions?: ComponentChildren[];   // buttons of the whole card; the caller passes only the ones that show
  actionsCls?: string;
  section?: string; renderId?: string; cls?: string; children?: ComponentChildren;
}

export function Card(p: CardProps) {
  const n = p.actions?.length ?? 0;
  return (
    <section class={`card${p.cls ? " " + p.cls : ""}`} data-section={p.section} data-render-id={p.renderId} id={p.section ? `sec-${p.section}` : undefined}>
      <header class="chead">
        <span class="tile"><Icon name={p.icon} /></span>
        <h2>{p.title}</h2>
        {p.sub ? <p>{p.sub}</p> : null}
        {n ? <div class={`acts n${n}${p.actionsCls ? " " + p.actionsCls : ""}`}>{p.actions}</div> : null}
      </header>
      {p.children}
    </section>
  );
}

// The band that opens a group of rows; a small action may sit at its right end (WiFi: Scan)
export function GroupBand({ title, action }: { title: string; action?: ComponentChildren }) {
  return <div class="gband"><h3>{title}</h3>{action ? <span class="acts">{action}</span> : null}</div>;
}

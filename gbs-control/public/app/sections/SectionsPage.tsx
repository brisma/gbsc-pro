// Pages with sections (Settings, System, Developer): the chosen section as one card, the same on PC and phone
// (sidebar sub-items on PC, tabs under the chain on the phone). Header band with the section's actions, a band per
// group, the rows; in Easy a last row says how many more settings Advanced has.
import { useMemo } from "preact/hooks";
import { Icon } from "../art/icon.tsx";
import { mode, pred } from "../store/app.ts";
import { route } from "../shell/router.ts";
import { visibleSections } from "../shell/sections.ts";
import type { Page, Section, Group } from "../layout/types.ts";
import { RowView, isRowVisible } from "../rows/Row.tsx";
import { rowState } from "../rows/resolve.ts";
import { desk } from "../store/app.ts";
import { Card, GroupBand } from "./Card.tsx";
import { SUBTITLES } from "./summaries.ts";

let renderSeq = 0;
// The section element keeps its identity across updates (every-row checks it: no re-mount, no replayed animation)
const useRenderId = () => useMemo(() => String(++renderSeq), []);

const rowKey = (r: Group["rows"][number]) => `${r.kind}:${r.ref ?? r.label}`;

// A group inside a card: its band, then its rows (the ones that are buttons of the card header are not repeated)
export function GroupView({ g, s, quick }: { g: Group; s?: Section; quick?: boolean }) {
  const rows = g.rows.filter((r) => !r.head && isRowVisible(r, g, s));
  if (!rows.length) return null;
  return (
    <>
      {g.title ? <GroupBand title={g.title} /> : null}
      {rows.map((r) => <RowView key={rowKey(r)} row={r} group={g} section={s} quick={quick} />)}
    </>
  );
}

// Rows of the section that Easy hides and Advanced shows
function hiddenInEasy(s: Section): number {
  if (mode.value !== "easy") return 0;
  const adv = { ...pred.value, menuMode: 1 };
  let n = 0;
  for (const g of s.groups ?? []) for (const r of g.rows)
    if (r.kind !== "note" && !(r.phoneOnly && desk.value) && !isRowVisible(r, g, s) && rowState(r, adv, g, s).visible) n++;
  return n;
}

function SectionCard({ s }: { s: Section }) {
  const id = useRenderId();
  const R = s.render;
  const head = (s.groups ?? []).flatMap((g) => g.rows.filter((r) => r.head && isRowVisible(r, g, s)).map((r) => ({ r, g })));
  const more = hiddenInEasy(s);
  const A = s.acts;
  return (
    <Card icon={s.icon} title={s.title} sub={SUBTITLES[s.id]?.() ?? s.hint} section={s.id} renderId={id}
          actions={[...head.map(({ r, g }) => <RowView key={rowKey(r)} row={r} group={g} section={s} head />), ...(A ? [<A key="acts" />] : [])]}>
      {R ? <R desk={desk.value} /> : (s.groups ?? []).map((g) => <GroupView key={g.title ?? g.rows[0]?.ref} g={g} s={s} />)}
      {more ? (
        <div class="more">
          <span class="ic"><Icon name="tune" /></span>
          <span>{more} more setting{more > 1 ? "s" : ""} in Advanced</span>
          <button type="button" onClick={() => { mode.value = "advanced"; }}>Show Advanced</button>
        </div>
      ) : null}
    </Card>
  );
}

export function SectionsPage({ page }: { page: Page }) {
  const secs = visibleSections(page).filter((x) => !x.to);
  if (!secs.length) return null;
  const s = secs.find((x) => x.id === route.value.section) ?? secs[0];
  return <SectionCard key={s.id} s={s} />;
}

// Sections shown in the current board and mode: a section of rows with none visible here is left out (the
// standard board in Easy has no Device rows, for example)
import type { PageDef, SectionDef } from "../layout/pages.ts";
import { board, desk, devOn } from "../store/app.ts";
import { isRowVisible } from "../rows/Row.tsx";

const hasRows = (s: SectionDef) => !!s.render || !s.groups || s.groups.some((g) => g.rows.some((r) => r.kind !== "note" && isRowVisible(r, g, s)));

export function visibleSections(p: PageDef): SectionDef[] {
  return p.sections.filter((s) => !(s.cap === "adv" && board.value !== "pro") && !(s.cap === "std" && board.value !== "standard") &&
    !(s.dev && !devOn.value) && !(s.phoneOnly && desk.value) && hasRows(s));
}

// PC sections: whole groups in two balanced columns. Fixed groups first, then the heaviest into the lighter column;
// each column keeps the original order. One group: its rows split in half into two cards, the right one untitled and
// marked cont; a short section may fill only the left column.

export interface Balanced<T> { cols: [T[], T[]] }

export function balance<T extends { rows: unknown[]; col?: 0 | 1; weight?: number; title?: string; cont?: boolean }>(groups: T[]): Balanced<T> {
  if (groups.length === 1) {
    const g = groups[0], h = Math.ceil(g.rows.length / 2);
    if (g.rows.length < 2) return { cols: [[g], []] };
    return { cols: [[{ ...g, rows: g.rows.slice(0, h) }], [{ ...g, rows: g.rows.slice(h), title: undefined, cont: true }]] };
  }
  const w = (g: T) => g.weight ?? g.rows.length + 1;   // + the title
  const cols: [{ g: T; i: number }[], { g: T; i: number }[]] = [[], []];
  const load = [0, 0];
  const items = groups.map((g, i) => ({ g, i }));
  for (const it of items.filter((x) => x.g.col !== undefined)) { cols[it.g.col!].push(it); load[it.g.col!] += w(it.g); }
  const free = items.filter((x) => x.g.col === undefined).sort((a, b) => w(b.g) - w(a.g) || a.i - b.i);
  for (const it of free) { const c = load[1] < load[0] ? 1 : 0; cols[c].push(it); load[c] += w(it.g); }
  const ordered = (l: { g: T; i: number }[]) => l.sort((a, b) => a.i - b.i).map((x) => x.g);
  return { cols: [ordered(cols[0]), ordered(cols[1])] };
}

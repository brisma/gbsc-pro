// ADV write and read plans: dev.adv_i2c carries <= 10 triplets; an ADV7280 map group (42,0E,<map> + writes + 42,0E,00)
// is never split, because the ADV Controller resets 0x0E every 50 ms; maps only on 0x42 (0x20, 0x40); 0x0E and
// read-only registers are never written; dangerous ones are listed for the caller to ask.
// dev.adv_read: <= 32 registers per request, never crossing 0xFF, one at a time.
import { advKey, advReg, type Regmap } from "./regmap.ts";

export interface AdvChange { i2c: number; map: number; reg: number; value: number }
export interface Skipped { reg: AdvChange; reason: string }
export interface WritePlan { requests: string[]; skipped: Skipped[]; dangerous: AdvChange[] }
export interface ReadRequest { dev: number; map: number; reg: number; count: number }

const MAX_TRIPLETS = 10;
const ADV7280_MAPS = [0x00, 0x20, 0x40];
const h = (n: number) => n.toString(16).toUpperCase().padStart(2, "0");
const triplet = (a: number, r: number, v: number) => [h(a), h(r), h(v)].join(",");

function refusal(c: AdvChange, m: Regmap): string | null {
  if (c.i2c !== 0x42 && c.map !== 0) return "no map on this chip";
  if (c.i2c === 0x42 && !ADV7280_MAPS.includes(c.map)) return "unknown map";
  if (m.danger.get(advKey(c.i2c, c.map, c.reg)) === "planner") return "map select";
  if (advReg(m, c.i2c, c.map, c.reg)?.readonly) return "read-only";
  return null;
}

export function planAdvWrites(changes: AdvChange[], m: Regmap): WritePlan {
  const merged = new Map<string, AdvChange>();   // the same register written twice: the last value, the first place
  for (const c of changes) {
    const k = advKey(c.i2c, c.map, c.reg);
    merged.set(k, { ...(merged.get(k) ?? c), value: c.value });
  }
  const skipped: Skipped[] = [], dangerous: AdvChange[] = [];
  const units: string[][] = [];                     // triplets that travel together
  const groups = new Map<number, AdvChange[]>();    // ADV7280 map -> its writes (placed at the first one)
  const order: (AdvChange | number)[] = [];
  for (const c of merged.values()) {
    const why = refusal(c, m);
    if (why) { skipped.push({ reg: c, reason: why }); continue; }
    if (m.danger.get(advKey(c.i2c, c.map, c.reg)) === "warn") dangerous.push(c);
    if (c.i2c === 0x42 && c.map !== 0) {
      if (!groups.has(c.map)) { groups.set(c.map, []); order.push(c.map); }
      groups.get(c.map)!.push(c);
    } else order.push(c);
  }
  for (const o of order) {
    if (typeof o !== "number") { units.push([triplet(o.i2c, o.reg, o.value)]); continue; }
    const g = groups.get(o)!;
    for (let i = 0; i < g.length; i += MAX_TRIPLETS - 2)
      units.push([triplet(0x42, 0x0E, o), ...g.slice(i, i + MAX_TRIPLETS - 2).map((c) => triplet(0x42, c.reg, c.value)), triplet(0x42, 0x0E, 0)]);
  }
  const requests: string[][] = [];
  for (const u of units) {
    const last = requests[requests.length - 1];
    if (last && last.length + u.length <= MAX_TRIPLETS) last.push(...u);
    else requests.push([...u]);
  }
  return { requests: requests.map((r) => r.join(",")), skipped, dangerous };
}

export function planAdvReads(regs: { i2c: number; map: number; reg: number }[]): ReadRequest[] {
  const seen = new Set<string>();
  const list = regs.filter((r) => { const k = advKey(r.i2c, r.map, r.reg); if (seen.has(k)) return false; seen.add(k); return true; })
    .sort((a, b) => a.i2c - b.i2c || a.map - b.map || a.reg - b.reg);
  const out: ReadRequest[] = [];
  for (const r of list) {
    const cur = out[out.length - 1];
    if (cur && cur.dev === r.i2c && cur.map === r.map && r.reg < cur.reg + 32) cur.count = r.reg - cur.reg + 1;
    else out.push({ dev: r.i2c, map: r.map, reg: r.reg, count: 1 });
  }
  return out;
}

// One request at a time (the device keeps one result slot); onEach sees every result in order
export async function runReads<T>(reqs: ReadRequest[], run: (q: ReadRequest) => Promise<T>, onEach?: (q: ReadRequest, v: T) => void): Promise<T[]> {
  const out: T[] = [];
  for (const q of reqs) {
    const v = await run(q);
    out.push(v);
    onEach?.(q, v);
  }
  return out;
}

// The register map of the Developer tools (GET /api/regmap, tools/regmap.py): TV5725 fields by name, ADV7280/ADV7391
// registers by map with their fields, defaults, read-only, dangerous and device-managed flags. Loaded once, when a
// register section opens (it is outside the page budget).
export type Access = "ro" | "confirm" | "rw";
export interface TvField { name: string; seg: number; reg: number; bit: number; width: number; access: Access }
export interface AdvField {
  name: string; lsb: number; msb: number; type: string; desc: string; values: Record<string, string>; range: unknown;
}
export type Danger = "" | "warn" | "planner";
export interface AdvReg {
  addr: number; name: string; desc: string; def: number; readonly: boolean; fields: AdvField[]; danger: Danger; managed: boolean;
}
export interface AdvMap { id: string; name: string; i2c: number; map: number; regs: AdvReg[] }
export interface AdvChip { id: string; addr: number; desc: string; maps: AdvMap[] }
export interface Regmap {
  tv5725: TvField[];
  chips: AdvChip[];
  macros: [string, string[]][];
  danger: Map<string, Danger>;   // advKey(i2c, map, reg) -> kind, also for registers the maps do not describe
}

const hex = (v: unknown): number => (typeof v === "number" ? v : parseInt(String(v ?? "0"), 16) || 0);
export const advKey = (i2c: number, map: number, reg: number) =>
  [i2c, map, reg].map((n) => n.toString(16).padStart(2, "0")).join(":");

type RawReg = [string, string, string, number, number, [string, number, number, string, string, Record<string, string>, unknown][], Danger, number];
interface Raw {
  version: number;
  tv5725: [string, number, number, number, number, Access][];
  adv: { chips: { id: string; addr: string; desc: string; maps: { id: string; name: string; mv: string | null; i2c: string | null; regs: RawReg[] }[] }[];
         macros: [string, string[]][]; danger: [number, number, number, Danger][] };
}

export function parseRegmap(raw: unknown): Regmap {
  const r = raw as Raw;
  const chips = r.adv.chips.map((c) => {
    const addr = hex(c.addr);
    return {
      id: c.id, addr, desc: c.desc,
      maps: c.maps.map((m) => {
        const i2c = m.i2c ? hex(m.i2c) : addr;
        return {
          id: m.id, name: m.name, i2c, map: i2c === 0x42 ? hex(m.mv) : 0,
          regs: m.regs.map((g) => ({
            addr: hex(g[0]), name: g[1], desc: g[2], def: g[3], readonly: !!g[4], danger: g[6] ?? "", managed: !!g[7],
            fields: g[5].map((f) => ({ name: f[0], lsb: f[1], msb: f[2], type: f[3], desc: f[4], values: f[5], range: f[6] })),
          })),
        };
      }),
    };
  });
  const danger = new Map<string, Danger>(r.adv.danger.map(([a, m, g, k]) => [advKey(a, m, g), k]));
  return { tv5725: r.tv5725.map(([name, seg, reg, bit, width, access]) => ({ name, seg, reg, bit, width, access })), chips, macros: r.adv.macros, danger };
}

export function advReg(m: Regmap, i2c: number, map: number, reg: number): AdvReg | undefined {
  for (const c of m.chips) for (const mp of c.maps) if (mp.i2c === i2c && mp.map === map) return mp.regs.find((x) => x.addr === reg);
  return undefined;
}

let loading: Promise<Regmap | null> | null = null;
// Once per page; null when the firmware was built without the map (404)
export function loadRegmap(get: (path: string) => Promise<Response> = (p) => fetch(p)): Promise<Regmap | null> {
  loading ??= get("/api/regmap").then(async (res) => (res.ok ? parseRegmap(await res.json()) : null)).catch(() => { loading = null; return null; });
  return loading;
}

// A TV5725 field from the bytes of its segment (little endian across registers, as tw.h regDecode); no shifts >= 31
export function fieldValue(seg: Uint8Array, f: TvField): number {
  let v = 0, mul = 1;
  const bytes = Math.ceil((f.bit + f.width) / 8);
  for (let i = 0; i < bytes; i++) { v += seg[f.reg + i] * mul; mul *= 256; }
  return Math.floor(v / 2 ** f.bit) % 2 ** f.width;
}

// The segment bytes with the field set to value; the other bits of every covered byte kept (read-modify-write)
export function withField(seg: Uint8Array, f: TvField, value: number): Uint8Array {
  const out = seg.slice();
  const bytes = Math.ceil((f.bit + f.width) / 8);
  for (let i = 0; i < bytes; i++) {
    const lo = Math.max(0, f.bit - 8 * i), hi = Math.min(8, f.bit + f.width - 8 * i);   // covered bits of byte i
    const mask = ((1 << hi) - 1) & ~((1 << lo) - 1) & 0xff;
    const shift = 8 * i - f.bit;   // value bit index of this byte's bit 0
    const part = shift >= 0 ? Math.floor(value / 2 ** shift) % 256 : (value * 2 ** -shift) % 256;
    out[f.reg + i] = (out[f.reg + i] & ~mask & 0xff) | (part & mask);
  }
  return out;
}

// TV5725 field writes: dev.tv5725_reg triplets (segment, register, value) for every byte a field covers
import { withField, type TvField } from "./regmap.ts";

const h = (n: number) => n.toString(16).toUpperCase().padStart(2, "0");

export function planTvWrite(f: TvField, value: number, seg: Uint8Array): string {
  const out = withField(seg, f, value);
  const bytes = Math.ceil((f.bit + f.width) / 8);
  const t: string[] = [];
  for (let i = 0; i < bytes; i++) t.push(h(f.seg), h(f.reg + i), h(out[f.reg + i]));
  return t.join(",");
}

// Every pending field of one segment applied to a working copy, then one triplet per changed register (several
// fields of one register become one write), <= 10 triplets per dev.tv5725_reg request
export function planTvWrites(changes: [TvField, number][], seg: Uint8Array): string[] {
  let work = seg;
  const touched = new Set<number>();
  for (const [f, v] of changes) {
    work = withField(work, f, v);
    for (let i = 0; i < Math.ceil((f.bit + f.width) / 8); i++) touched.add(f.reg + i);
  }
  const t = [...touched].sort((x, y) => x - y).map((r) => [h(changes[0][0].seg), h(r), h(work[r])].join(","));
  const out: string[] = [];
  for (let i = 0; i < t.length; i += 10) out.push(t.slice(i, i + 10).join(","));
  return out;
}

// ADV presets and macros on the page: a preset from the values applied now (only what differs from
// Default, everything, one chip), the writes a preset or macro makes, macro steps turned into w / wm lines (an option
// step writes its whole register from the known value), and the name an imported file without one gets.
import { advKey, type Regmap, type AdvMap } from "./regmap.ts";
import { type PresetDoc, type MacroDoc, type PresetMap } from "./yaml.ts";
import type { AdvChange } from "./planner.ts";

export type PresetScope = "changed" | "all" | "dec" | "enc";
export type MacroStep =
  | { kind: "field"; i2c: number; map: number; reg: number; lsb: number; x: number }
  | { kind: "reg"; i2c: number; map: number; reg: number; value: number };

const writable = (m: AdvMap) => m.regs.filter((r) => !r.readonly && r.danger !== "planner");
const h = (n: number) => n.toString(16).toUpperCase().padStart(2, "0");

export function presetFromApplied(rm: Regmap, values: Map<string, number>, scope: PresetScope, name: string, description: string): PresetDoc {
  const devices: PresetDoc["devices"] = [];
  for (const c of rm.chips) {
    if ((scope === "dec" && c.id !== "ADV7280") || (scope === "enc" && c.id !== "ADV7391")) continue;
    const maps: PresetMap[] = [];
    for (const m of c.maps) {
      const regs: [number, number][] = [];
      for (const r of writable(m)) {
        const v = values.get(advKey(m.i2c, m.map, r.addr));
        if (v === undefined || (scope === "changed" && v === r.def)) continue;
        regs.push([r.addr, v]);
      }
      if (regs.length) maps.push({ id: m.id, mapValue: m.map, regs });
    }
    if (maps.length) devices.push({ chip: c.id, maps });
  }
  return { kind: "preset", name, description, devices };
}

export function defaultPreset(rm: Regmap): PresetDoc {
  const values = new Map<string, number>();
  for (const c of rm.chips) for (const m of c.maps) for (const r of m.regs) values.set(advKey(m.i2c, m.map, r.addr), r.def);
  return presetFromApplied(rm, values, "all", "Default", "The register map's default values");
}

export function presetChanges(doc: PresetDoc, rm: Regmap): AdvChange[] {
  const out: AdvChange[] = [];
  for (const d of doc.devices) {
    const c = rm.chips.find((x) => x.id === d.chip);
    for (const pm of d.maps) {
      const m = c?.maps.find((x) => x.id === pm.id);
      const i2c = m ? m.i2c : c?.addr ?? 0, map = m ? m.map : pm.mapValue;
      if (!i2c) continue;
      for (const [reg, value] of pm.regs) out.push({ i2c, map, reg, value });
    }
  }
  return out;
}

export const macroChanges = (doc: MacroDoc): AdvChange[] => doc.commands.filter((c) => c.run && c.write).map((c) => c.write!);

export function stepsToCommands(steps: MacroStep[], rm: Regmap, values: Map<string, number>): string[] {
  const cur = new Map<string, number>();
  return steps.map((s) => {
    const k = advKey(s.i2c, s.map, s.reg);
    let v: number;
    if (s.kind === "reg") v = s.value;
    else {
      const reg = rm.chips.flatMap((c) => c.maps).find((m) => m.i2c === s.i2c && m.map === s.map)?.regs.find((r) => r.addr === s.reg);
      const f = reg?.fields.find((x) => x.lsb === s.lsb);
      const base = cur.get(k) ?? values.get(k) ?? reg?.def ?? 0;
      const width = f ? f.msb - f.lsb + 1 : 1, mask = ((1 << width) - 1) << s.lsb;
      v = (base & ~mask) | ((s.x << s.lsb) & mask);
    }
    cur.set(k, v);
    return s.i2c === 0x42 && s.map !== 0 ? `wm 42 ${h(s.map)} ${h(s.reg)} ${h(v)}` : `w ${h(s.i2c)} ${h(s.reg)} ${h(v)}`;
  });
}

// An ADV Manager file must carry a name: one without it is named after the file
export function withName(text: string, fileName: string): string {
  const line = /^name:(.*)$/m.exec(text.replace(/\r/g, ""));
  if (line && line[1].trim().replace(/^(['"])(.*)\1$/, "$2").trim()) return text;
  const stem = fileName.replace(/\.ya?ml$/i, "").slice(0, 32) || "Imported";
  const name = /^[A-Za-z0-9][A-Za-z0-9 _().,+/-]*$/.test(stem) ? stem : `'${stem.replace(/'/g, "''")}'`;
  return `name: ${name}\n${text.replace(/^name:\s*(''|"")?\s*\n/m, "")}`;
}

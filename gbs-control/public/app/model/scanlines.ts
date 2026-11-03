// Scanlines: two settings, picture.scanlines on/off and picture.scanline_strength. Easy = the choice table
// of menu.json (+ "Custom" when the strength matches none); Advanced = Off + the strength range in the menu node's
// steps, labelled by the high hex digit (0x10 -> "10"). Nothing here is hard-coded.
import { menu, nodeById, registry, type Mode } from "./menu.ts";
import { choiceValue, choiceWrites } from "./choices.ts";

type Values = Record<string, number | string>;
const ON = "picture.scanlines", STRENGTH = "picture.scanline_strength";
const CUSTOM = "Custom";

function strengthSteps(): number[] {
  const e = registry.entry(STRENGTH);
  const step = nodeById("picture/scanline-strength")?.step ?? 1;
  const out: number[] = [];
  for (let v = Math.max(e?.min ?? 0, step); v <= (e?.max ?? 0); v += step) out.push(v);
  return out;
}
const hexLabel = (v: number) => v.toString(16).toUpperCase().padStart(2, "0");
const offLabel = () => menu.choices.scanlines[0].label;

export function scanlineValue(mode: Mode, values: Values): string {
  if (Number(values[ON] ?? 0) === 0) return offLabel();
  if (mode === "advanced") {
    const v = Number(values[STRENGTH] ?? 0);
    return strengthSteps().includes(v) ? hexLabel(v) : CUSTOM;
  }
  const i = choiceValue("scanlines", values);
  return i >= 0 ? menu.choices.scanlines[i].label : CUSTOM;
}

export function scanlineOptions(mode: Mode, values?: Values): { label: string; strength: number | null }[] {
  const out = mode === "advanced"
    ? [{ label: offLabel(), strength: null }, ...strengthSteps().map((v) => ({ label: hexLabel(v), strength: v }))]
    : menu.choices.scanlines.map((o) => ({ label: o.label, strength: o.writes.find((w) => w.key === STRENGTH && Number(o.writes[0].value))?.value ?? null }));
  if (values && scanlineValue(mode, values) === CUSTOM) out.push({ label: CUSTOM, strength: Number(values[STRENGTH] ?? 0) });
  return out;
}

export function scanlineWrites(mode: Mode, label: string): [string, number][] {
  if (label === offLabel()) return [[ON, 0]];
  if (mode === "advanced") {
    const v = strengthSteps().find((s) => hexLabel(s) === label);
    return v === undefined ? [] : [[ON, 1], [STRENGTH, v]];
  }
  const i = menu.choices.scanlines.findIndex((o) => o.label === label);
  return i < 0 ? [] : choiceWrites("scanlines", i);
}

/** How dark a strength looks on a card thumbnail, on the named cards' scale (Light 10 .. Strong 50): lower strength
 *  values draw darker lines. */
export function scanlineDarkness(strength: number): number {
  const max = registry.entry(STRENGTH)?.max ?? 0x60;
  return ((max - strength) / 16) * 10;
}

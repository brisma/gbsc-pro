// Choices (Filter, Scanlines, Resolution): rows that write two settings at once, ports of menuChoiceCurrent,
// choiceOffered and menuChoiceWrites (pro/ui/menu-model.cpp)
import { menu, type Mode } from "./menu.ts";
import { CAP } from "./predicates.ts";

type Values = Record<string, number | string>;
export interface ChoiceCtx { basePreset?: number }

const OUTPUT_CUSTOMIZED = 2;   // options.h OutputCustomized: with a profile loaded the choice shows its base preset

function read(values: Values, key: string, c: ChoiceCtx): number {
  const v = Number(values[key] ?? -1);
  return key === "output.preset" && v === OUTPUT_CUSTOMIZED && c.basePreset !== undefined ? c.basePreset : v;
}

// Index of the current option (an alias reads as its main option), -1 = Custom
export function choiceValue(choice: string, values: Values, c: ChoiceCtx = {}): number {
  const opts = menu.choices[choice] ?? [];
  for (let i = 0; i < opts.length; i++) {
    const o = opts[i];
    const [w1, w2] = o.writes;
    if (read(values, w1.key, c) !== w1.value) continue;
    if (w2 && !o.flags.includes("sid1_only") && read(values, w2.key, c) !== w2.value) continue;
    if (!o.flags.includes("alias")) return i;
    const main = opts.findIndex((k) => k.label === o.label && !k.flags.includes("alias"));
    if (main >= 0) return main;
  }
  return -1;
}

export function choiceWrites(choice: string, index: number): [string, number][] {
  const o = menu.choices[choice]?.[index];
  if (!o) return [];
  const w = o.flags.includes("sid1_only") ? o.writes.slice(0, 1) : o.writes;
  return w.map((x) => [x.key, x.value]);
}

// The options a row offers (never an alias; Original-only on the standard board; Advanced-only in Advanced), plus
// the current one when it is not offered, as menuChoiceStep keeps it
export function choiceOptions(choice: string, c: { mode: Mode; caps: number; current?: number }): { index: number; label: string }[] {
  const opts = menu.choices[choice] ?? [];
  return opts
    .map((o, index) => ({ o, index }))
    .filter(({ o, index }) => index === c.current || (!o.flags.includes("alias") &&
            !(o.flags.includes("original") && !(c.caps & CAP.ORIGINAL)) && !(o.flags.includes("advanced") && c.mode === "easy")))
    .map(({ o, index }) => ({ index, label: o.label }));
}

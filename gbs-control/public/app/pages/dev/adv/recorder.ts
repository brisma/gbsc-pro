// "Record what I apply next" (macro editor): while a draft records, Apply in Decoder or Encoder hands its writes here
import { signal } from "@preact/signals";
import type { AdvChange } from "../../../regs/planner.ts";

export const recording = signal<((changes: AdvChange[]) => void) | null>(null);

export function recordApplied(changes: AdvChange[]): void {
  const take = recording.value;
  if (!take) return;
  recording.value = null;
  take(changes);
}

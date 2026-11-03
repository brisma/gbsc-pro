// The open accordion (cards, pickers, pads, inline links): one at a time, kept outside the row components so a
// re-render that moves a group to the other PC column does not close it
import { signal } from "@preact/signals";

export const openAcc = signal<string | null>(null);

export function useAcc(id: string): [boolean, () => void, () => void] {
  const open = openAcc.value === id;
  return [open, () => { openAcc.value = open ? null : id; }, () => { if (openAcc.value === id) openAcc.value = null; }];
}

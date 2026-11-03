// The "?" of a row: the longer web text (app/texts/help.ts), else the menu's short hint
import type { VNode } from "preact";
import { openSheet, closeSheet, SheetButtons } from "./Sheet.tsx";

export interface HelpSpec { title: string; icon: string; text: string; picture?: () => VNode; where?: string }

export function openHelp(h: HelpSpec): void {
  openSheet({
    title: h.title, icon: h.icon,
    body: () => (
      <>
        {h.picture?.()}
        <p>{h.text}</p>
        {h.where ? <div class="meta">{h.where}</div> : null}
        <SheetButtons><button type="button" class="btn" onClick={closeSheet}>Close</button></SheetButtons>
      </>
    ),
  });
}

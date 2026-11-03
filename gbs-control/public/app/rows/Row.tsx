// One row of a group: resolves its state (hidden / greyed with the TV reason) and draws the control of its kind
import { pred, desk } from "../store/app.ts";
import type { Row as RowDef, Group, Section } from "../layout/types.ts";
import { rowState, rowLabel } from "./resolve.ts";
import { ToggleRow, StepperRow, ChoiceRow, ActionRow, LiveRow, LinkRow, NoteRow, SecretRow } from "./controls.tsx";
import { CardsRow } from "./Cards.tsx";
import { PickerRow } from "./Picker.tsx";
import { PadRow } from "./Pad.tsx";

export interface RowViewProps { row: RowDef; group?: Group; section?: Section; quick?: boolean; head?: boolean }

export function RowView({ row, group, section, quick, head }: RowViewProps) {
  const st = rowState(row, pred.value, group, section);
  if (!st.visible || (row.phoneOnly && desk.value)) return null;
  const p = { row, label: rowLabel(row), reason: st.reason };
  switch (row.kind) {
    case "toggle": return <ToggleRow {...p} />;
    case "stepper": return <StepperRow {...p} />;
    case "inline": case "menu": return <ChoiceRow {...p} />;
    case "cards": return <CardsRow {...p} />;
    case "picker": return <PickerRow {...p} />;
    case "pad": return <PadRow {...p} />;
    case "action": return <ActionRow {...p} head={head} />;
    case "live": return <LiveRow {...p} />;
    case "link": return <LinkRow {...p} />;
    case "note": return <NoteRow row={row} />;
    case "secret": return <SecretRow {...p} />;
    default: return null;
  }
}

export function isRowVisible(row: RowDef, group?: Group, section?: Section): boolean {
  return rowState(row, pred.value, group, section).visible && !(row.phoneOnly && desk.value);
}

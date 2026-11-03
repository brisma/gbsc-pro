// Arrow pads (Position, Size, Edge mask, Mask position): two steppers, across and down, with the readout from
// /api/live between the arrows; an arrow held repeats the menu's adjust action (menu.json "adjust")
import { Icon } from "../art/icon.tsx";
import { menu } from "../model/menu.ts";
import { formatLive } from "../model/format.ts";
import { useHold } from "../shell/hold.ts";
import { Frame } from "./Frame.tsx";
import { rowNode } from "./resolve.ts";
import { runAction, useLive, type RowProps } from "./controls.tsx";

const ARROW: Record<string, string> = { up: "keyboard_arrow_up", down: "keyboard_arrow_down", left: "keyboard_arrow_left", right: "keyboard_arrow_right" };   // @icons

function Arrow({ dir, action, label, disabled }: { dir: "up" | "down" | "left" | "right"; action: string; label: string; disabled: boolean }) {
  const hold = useHold(() => void runAction(action));
  return <button type="button" class="a" aria-label={`${label} ${dir}`} disabled={disabled} {...hold}><Icon name={ARROW[dir]} /></button>;
}

export function PadRow({ row, label, reason }: RowProps) {
  const node = rowNode(row);
  const a = menu.adjust[node?.adjust ?? ""];
  const v = useLive(node?.live);
  const [x, y] = (node?.live ? formatLive(node.live, v) : "").split("/");
  if (!a) return null;
  return (
    <Frame row={row} label={label} reason={reason} wide>
      <div class="pad2">
        <div class="stp" role="group" aria-label={`${label}, across`}>
          <Arrow dir="left" action={a.left} label={label} disabled={!!reason} /><b aria-live="polite">{x ?? ""}</b><Arrow dir="right" action={a.right} label={label} disabled={!!reason} />
        </div>
        <div class="stp" role="group" aria-label={`${label}, down`}>
          <Arrow dir="up" action={a.up} label={label} disabled={!!reason} /><b aria-live="polite">{y ?? ""}</b><Arrow dir="down" action={a.down} label={label} disabled={!!reason} />
        </div>
      </div>
    </Frame>
  );
}

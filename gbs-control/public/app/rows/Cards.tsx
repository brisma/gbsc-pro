// Look choices (Filter, Scanlines, Deinterlace, HDMI range): segments, one click each
import { device, mode, pred } from "../store/app.ts";
import { choiceValue, choiceWrites, choiceOptions } from "../model/choices.ts";
import { scanlineOptions, scanlineValue, scanlineWrites } from "../model/scanlines.ts";
import { Frame } from "./Frame.tsx";
import { rowNode, rowSetting } from "./resolve.ts";
import { rowOptions, Seg, segWide, type RowProps, type SegOpt } from "./controls.tsx";

function useOptions(p: RowProps): SegOpt[] {
  const node = rowNode(p.row);
  const values = device.values.value;
  const writeAll = (pairs: [string, number][]) => { if (pairs.length) void device.writeSettings(pairs); };
  if (p.row.look === "scanlines") {
    const cur = scanlineValue(mode.value, values);
    return scanlineOptions(mode.value, values).map((o) => ({ label: o.label, on: o.label === cur,
      pick: () => { if (o.label !== cur && o.label !== "Custom") writeAll(scanlineWrites(mode.value, o.label)); } }));
  }
  if (node?.type === "choice" && node.choice) {
    const c = node.choice, idx = choiceValue(c, values);
    return choiceOptions(c, { mode: mode.value, caps: pred.value.caps, current: idx }).map((o) => ({ label: o.label, on: o.index === idx,
      pick: () => { if (o.index !== idx) writeAll(choiceWrites(c, o.index)); } }));
  }
  const key = rowSetting(p.row) ?? "";
  const cur = Number(values[key] ?? 0);
  return rowOptions(node, key).map((o) => ({ label: o.n, on: o.v === cur, pick: () => { if (o.v !== cur) writeAll([[key, o.v]]); } }));
}

export function CardsRow(p: RowProps) {
  const opts = useOptions(p);
  return <Frame row={p.row} label={p.label} reason={p.reason} wide={segWide(opts.map((o) => o.label))}><Seg label={p.label} opts={opts} disabled={!!p.reason} /></Frame>;
}

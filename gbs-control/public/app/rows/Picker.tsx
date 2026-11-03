// Main choices (Source cable, Resolution). PC: a strip of pictures across the row, one click each. Phone: the row
// shows the choice with its picture; a tap opens the list under it and a choice closes it.
import { useAcc } from "./accordion.ts";
import { Icon } from "../art/icon.tsx";
import { connectorUrl } from "../art/connectors.tsx";
import { device, mode, pred, desk } from "../store/app.ts";
import { choiceValue, choiceWrites, choiceOptions } from "../model/choices.ts";
import { registry } from "../model/menu.ts";
import { T } from "../texts/ui.ts";
import { Frame } from "./Frame.tsx";
import { rowNode } from "./resolve.ts";
import { rowOptions, type RowProps } from "./controls.tsx";
import { RES_HELP } from "./looks.tsx";

const cableName = (v: number) => rowOptions(undefined, "input.source").find((o) => o.v === v)?.n ?? "";
const signalText = () => {
  const sig = device.info.value?.signal;
  return sig && !sig.none ? `${sig.lines}${sig.interlaced ? "i" : "p"} ${Math.round(Number(sig.src_hz10) / 10)} Hz` : "";
};
const Check = () => <Icon name="check_circle" cls="ck" />;

// Phone: the cables as a list (picture, name, what it carries now)
function SourceList({ onPick }: { onPick: () => void }) {
  const choice = Number(device.values.value["input.source"] ?? 0);
  const info = device.info.value;
  const active = Number(info?.input?.active ?? 0);
  const searching = info?.input?.state === "searching";
  const cables = rowOptions(undefined, "input.source").filter((o) => o.v > 0);
  const pick = (v: number) => { if (v !== choice) void device.writeSettings([["input.source", v]]); onPick(); };
  return (
    <div class="plist" role="listbox" aria-label="Source">
      <button type="button" class={`po${choice === 0 ? " on" : ""}`} role="option" aria-selected={choice === 0} onClick={() => pick(0)}>
        <span class="pi"><Icon name="autorenew" /></span><span><b>{T.auto}</b><small>{T.autoHint}</small></span>{choice === 0 ? <Check /> : <span />}
      </button>
      {cables.map((c) => {
        const on = c.v === active && !searching;
        return (
          <button type="button" class={`po${c.v === choice ? " on" : ""}`} role="option" aria-selected={c.v === choice} onClick={() => pick(c.v)}>
            <span class="pi"><img alt="" src={connectorUrl(c.v)} /></span>
            <span><b>{c.n}</b><small class={on ? "sig" : ""}>{on ? signalText() || T.signal : "no signal"}</small></span>{c.v === choice ? <Check /> : <span />}
          </button>
        );
      })}
    </div>
  );
}

function resolutions() {
  const idx = choiceValue("resolution", device.values.value, { basePreset: device.profiles.value?.base });   // a profile: its base resolution, as on the TV
  const opts = choiceOptions("resolution", { mode: mode.value, caps: pred.value.caps, current: idx });
  const pick = (index: number) => { if (index !== idx) void device.writeSettings(choiceWrites("resolution", index)); };
  return { idx, opts, pick };
}

function ResolutionList({ onPick }: { onPick: () => void }) {
  const { idx, opts, pick } = resolutions();
  return (
    <div class="plist" role="listbox" aria-label="Resolution">
      {opts.map((o) => {
        const [t, d] = RES_HELP[o.label] ?? ["", ""];
        return (
          <button type="button" class={`po noimg${o.index === idx ? " on" : ""}`} role="option" aria-selected={o.index === idx} onClick={() => { pick(o.index); onPick(); }}>
            <span><b>{o.label}</b><small>{[t, d].filter(Boolean).join(", ")}</small></span>{o.index === idx ? <Check /> : <span />}
          </button>
        );
      })}
    </div>
  );
}

function ResolutionStrip() {
  const { idx, opts, pick } = resolutions();
  return (
    <div class="rstrip" style={`grid-template-columns:repeat(${opts.length},minmax(0,1fr))`} role="radiogroup" aria-label="Resolution">
      {opts.map((o) => {
        const [t, d] = RES_HELP[o.label] ?? ["", ""];
        return (
          <button type="button" class={`rcard${o.index === idx ? " sel" : ""}`} role="radio" aria-checked={o.index === idx} onClick={() => pick(o.index)}>
            <b class="rv">{o.label}</b><b>{t}</b><small>{d}</small>
          </button>
        );
      })}
    </div>
  );
}

export function sourceText(): string {
  const choice = Number(device.values.value["input.source"] ?? 0);
  const info = device.info.value;
  if (choice !== 0) return cableName(choice);
  if (info?.input?.state === "searching") return T.searching;
  return info?.input?.active ? `Auto · ${cableName(Number(info.input.active))}` : T.auto;
}

export function PickerRow(p: RowProps) {
  const [open, flip, close] = useAcc(`picker:${p.row.ref}`);
  const isSource = p.row.look === "source" || rowNode(p.row)?.setting === "input.source";
  if (desk.value) return <Frame row={p.row} label={p.label} reason={p.reason} two>{isSource ? <SourceStrip /> : <ResolutionStrip />}</Frame>;
  const choice = Number(device.values.value["input.source"] ?? 0);
  const active = Number(device.info.value?.input?.active ?? 0);
  const shown = choice || active;
  const { idx, opts } = resolutions();
  const resLabel = opts.find((o) => o.index === idx)?.label ?? T.custom;
  const toggle = () => { if (!p.reason) flip(); };
  return (
    <Frame row={p.row} label={p.label} reason={p.reason} cls="pick" onRow={toggle} open={open}
           panel={open ? (isSource ? <SourceList onPick={close} /> : <ResolutionList onPick={close} />) : null}>
      <button type="button" class="pv" aria-expanded={open} aria-label={`${p.label}: ${isSource ? sourceText() : resLabel}`} disabled={!!p.reason}
              onClick={(e) => { e.stopPropagation(); toggle(); }}>
        {isSource && shown ? <img alt="" src={connectorUrl(shown)} /> : null}
        {isSource ? <span>{choice ? cableName(choice) : T.auto}{!choice && active ? <small>{cableName(active)}</small> : null}</span> : <span>{resLabel}</span>}
        <Icon name={open ? "expand_less" : "expand_more"} />
      </button>
    </Frame>
  );
}

// PC Settings › Input: Auto and the cables in one strip, the signal per cable and the "in Auto" switches (the
// menu's Auto search page: input.scan_*)
const SCAN_KEY: Record<number, string> = { 1: "input.scan_rgbs", 2: "input.scan_rgsb", 3: "input.scan_vga", 4: "input.scan_yuv", 5: "input.scan_sv", 6: "input.scan_av" };

export function SourceStrip() {
  const values = device.values.value;
  const choice = Number(values["input.source"] ?? 0);
  const info = device.info.value;
  const active = Number(info?.input?.active ?? 0);
  const searching = info?.input?.state === "searching";
  const sigText = signalText();
  const cables = rowOptions(undefined, "input.source").filter((o) => o.v > 0);
  const pick = (v: number) => { if (v !== choice) void device.writeSettings([["input.source", v]]); };
  const scanVisible = (v: number) => {
    const e = registry.entry(SCAN_KEY[v]);
    return !!e && ((e.caps ?? 0) & pred.value.caps) === (e.caps ?? 0);
  };
  return (
    <div class="strip7" style={`grid-template-columns:repeat(${cables.length + 1},minmax(0,1fr))`} role="radiogroup" aria-label="Source">
      <div class={`scab${choice === 0 ? " sel" : ""}`}>
        <button type="button" class="hit" role="radio" aria-checked={choice === 0} aria-label="Auto" onClick={() => pick(0)} />
        <Icon name="autorenew" /><b>{T.auto}</b><small>by itself</small>
      </div>
      {cables.map((c) => {
        const on = c.v === active && !searching;
        const key = SCAN_KEY[c.v];
        const scanOn = Number(values[key] ?? 0) !== 0;
        return (
          // the card is a frame: a transparent radio covers it, the In Auto switch sits on top as its sibling (no nested
          // controls, axe nested-interactive)
          <div class={`scab${c.v === choice ? " sel" : ""}${on ? " on" : ""}`}>
            <button type="button" class="hit" role="radio" aria-checked={c.v === choice} aria-label={c.n} aria-describedby={`sig-${c.v}`} onClick={() => pick(c.v)} />
            <img alt="" src={connectorUrl(c.v)} /><b>{c.n}</b><small id={`sig-${c.v}`} class={on ? "sig" : ""}>{on ? sigText || T.signal : "no signal"}</small>
            {scanVisible(c.v) ? (
              <span class="scat">
                <button type="button" role="switch" aria-checked={scanOn} aria-label={`${c.n} in Auto`} class={`tg${scanOn ? " on" : ""}`}
                        onClick={() => void device.writeSettings([[key, scanOn ? 0 : 1]])} />
                <i>in Auto</i>
              </span>
            ) : null}
          </div>
        );
      })}
    </div>
  );
}

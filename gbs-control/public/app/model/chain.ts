// The signal chain shown on top of every page: Input -> Scaler -> Picture -> Output. Pure: the view resolves the
// labels (cable, resolution, filter, scanlines) with the menu helpers and passes them in.

export type ChainState = "ok" | "search" | "lost" | "off";
export type CableState = "ok" | "search" | "off";
export type StageId = "input" | "scaler" | "picture" | "output";

export interface ChainSignal { none?: number | boolean; lines?: number; interlaced?: number | boolean; src_hz10?: number; out_hz10?: number; preset?: number }

// The preset running: from 0x21 the bypass presets (HD bypass, VGA pass-through with VGA upscaling off), where the
// output is the source whatever the Resolution choice says
export function isBypass(sig: ChainSignal | null | undefined): boolean { return Number(sig?.preset ?? 0) >= 0x21; }

// "240p 60 Hz"; the rate left out while it is not measured (the VGA pass-through never measures it); "" without a signal
export function signalText(sig: ChainSignal | null | undefined): string {
  if (!sig || sig.none) return "";
  const hz = Number(sig.src_hz10 ?? 0);
  return `${sig.lines}${sig.interlaced ? "i" : "p"}${hz ? ` ${Math.round(hz / 10)} Hz` : ""}`;
}

// A measured rate under 55 Hz: PAL. No rate is not PAL
export function palRate(srcHz10: number | undefined): boolean { const v = Number(srcHz10 ?? 0); return v > 0 && v < 550; }
export interface ChainIn {
  offline: boolean;            // no link to the device
  searching: boolean;          // Auto is trying the inputs
  signal: ChainSignal | null | undefined;
  cable: string;               // name of the input in use (or being tried)
  resolution: string;          // the Resolution choice as the menu prints it ("960p/1024p", "1080p", "Custom")
  filter: string;              // "Normal", "Sharp", "Smooth", "Custom"
  scanlines: string;           // "Off", "Light", "30", "Custom"
  frameLock: boolean;
  ypbpr: boolean;              // analog out: YPbPr instead of RGB
}
export interface Stage { id: StageId; name: string; value: string; detail: string; tone?: "warn" | "bad"; dim?: boolean }
export interface Chain { state: ChainState; stages: Stage[]; cables: [CableState, CableState, CableState] }

// Lines the scaler sends for a Resolution choice: "960p/1024p" is 960 at 60 Hz and 1024 at 50 Hz
export function outputLines(resolution: string, srcHz10: number): number | null {
  const n = [...resolution.matchAll(/(\d{3,4})p/g)].map((m) => Number(m[1]));
  if (!n.length) return null;
  return n.length > 1 && srcHz10 > 0 && srcHz10 < 550 ? n[1] : n[0];
}

export function upscaleText(inLines: number, outLines: number | null): string {
  if (!(inLines > 0) || !outLines) return "Upscale";
  const f = outLines / inLines;
  return `Upscale ×${Number.isInteger(f) ? f : f.toFixed(1)}`;
}

export function scanlinesText(v: string): string {
  if (/^off$/i.test(v)) return "Scanlines off";
  return /^\d/.test(v) ? `Scanlines ${v}` : `${v} scanlines`;
}

export function chain(i: ChainIn): Chain {
  const sig = i.signal;
  const state: ChainState = i.offline ? "off" : i.searching ? "search" : !sig || sig.none ? "lost" : "ok";
  const ok = state === "ok";
  const lines = Number(sig?.lines ?? 0), srcHz = Number(sig?.src_hz10 ?? 0), outHz = Number(sig?.out_hz10 ?? 0);
  const bypass = ok && isBypass(sig);
  const out = ok && !bypass ? outputLines(i.resolution, srcHz) : null;
  const analog = i.ypbpr ? "YPbPr" : "RGB";
  const stages: Stage[] = [
    { id: "input", name: "Input", value: i.cable,
      detail: ok ? signalText(sig) : state === "search" ? "trying…" : state === "lost" ? "no signal" : "not connected",
      tone: state === "search" ? "warn" : state === "lost" ? "bad" : undefined },
    bypass ? { id: "scaler", name: "Scaler", value: "Bypass", detail: "no scaling" }
           : { id: "scaler", name: "Scaler", value: upscaleText(ok ? lines : 0, out), detail: `Frame lock ${i.frameLock ? "on" : "off"}` },
    { id: "picture", name: "Picture", value: `${i.filter} filter`, detail: scanlinesText(i.scanlines) },
    { id: "output", name: "Output", value: bypass ? "Same as input" : out ? `${out}p` : i.resolution, detail: ok && outHz ? `${(outHz / 10).toFixed(1)} Hz, ${analog}` : analog, dim: !ok },
  ];
  const cables: Chain["cables"] = ok ? ["ok", "ok", "ok"] : state === "search" ? ["search", "off", "off"] : ["off", "off", "off"];
  return { state, stages, cables };
}

// The stage a Settings section belongs to (its icon gets the ring)
export const STAGE_OF_SECTION: Record<string, StageId> = { input: "input", scaler: "scaler", picture: "picture", output: "output" };

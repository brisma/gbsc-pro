// The simulated device: settings values, revision, console log ring, event ring, runtime state and /api/info
import type { Gen, Board, RegEntry } from "./types.ts";
import { BOARD_CAPS } from "./types.ts";
import { findSetting, available } from "./validate.ts";
import { compareVersions } from "../app/model/version.ts";   // the device's frozen version order (update-manifest.cpp)

export interface LogRecord { s: number; t: number; c: string; l: string; m: string }
export interface Event { seq: number; ms: number; type: string; a: number; b: number }

// logFormatLine (pro/log/log-ring.cpp): "[%6lu.%03lu] %-10s %s\n"
export function formatLogLine(r: LogRecord): string {
  const sec = String(Math.floor(r.t / 1000)).padStart(6, " ");
  const ms = String(r.t % 1000).padStart(3, "0");
  return `[${sec}.${ms}] ${`[${r.c}]`.padEnd(10, " ")} ${r.m}\n`;
}

const seqAfter = (a: number, b: number) => ((a - b) & 0xffff) !== 0 && ((a - b) & 0xffff) < 0x8000;
const nextSeq = (s: number) => (s >= 65535 ? 1 : s + 1);

export const LIVE_DEFAULTS: Record<string, number | [number, number]> = {
  // read on a Pro with a PAL Mega Drive on RGB SCART; out_rate 600 here (the scenario's 60 Hz output)
  adc_gain: 132, position: [140, 44], size: [585, 512], borders: [2006, 1064], border_pos: [308, 44], htotal: 2035, pll_div: 2269, sdram: 2, osr: 4,
  adc_filter: 3, debug_view: 0, freeze: 0, if_offset: 0, sog: 13, out_rate: 600, syncwatcher: 1, osd_always_on: 0,
  wifi_rssi: -58, osd_test: 0, print_info: 0, ir_log: 0, sync_polarity: 0, ota: 0,
};

export class DeviceState {
  gen: Gen;
  board: Board;
  values = new Map<string, number | string>();
  settingsRev = 1;
  boot = 1 + Math.floor(Math.random() * 65534);
  started = Date.now();
  logs: LogRecord[] = [];
  private logSeq = 1;
  events: Event[] = [];
  private eventSeq = 1;
  runtime = { activeInput: 1, termination: false, presetId: 0x11, noSignal: false, interlaced: false, scanlinesOk: true,
              advNtsc: false, srcLines: 240, srcHz10: 600, outHz10: 600, std: 2, searching: false };
  // GET /api/live (api-live.cpp names): the menu readbacks; pairs as [h, v]
  live: Record<string, number | [number, number]> = { ...LIVE_DEFAULTS };
  fw = "3.0.0-dev0";
  wifi = { mode: "STA" as "STA" | "AP", ssid: "Home", ip: "192.168.1.50", scanned: false };
  upload: { state: string; error: string | null; received: number; total: number } = { state: "idle", error: null, received: 0, total: 0 };
  tvRegs = { state: "idle", seg: 0, gen: 0 };
  tvTable: Uint8Array[] = Array.from({ length: 6 }, (_, seg) => Uint8Array.from({ length: 256 }, (_, r) => (seg * 37 + r * 13) & 0xff));
  advTable = new Map<string, number>();   // "dev:map:reg" -> value written by dev.adv_i2c (else the seeded advReg)
  advValue(dev: number, map: number, reg: number): number {
    return this.advTable.get(`${dev}:${map}:${reg}`) ?? ((dev ^ map ^ (reg * 7)) & 0xff);
  }
  advRead = { state: "idle", dev: 0, map: 0, reg: 0, count: 0, gen: 0, announced: 0 };
  advCaps = ["signal", "reg_read"];
  advStatus = "ok";
  advUpdate = false;
  updateOffer: { version: string; channel: string } | null = null;        // the recommended offer (RAM on the device)
  serverOffer: { version: string; channel: string } | null = null;        // what a check finds on the update server
  manualOffer: { version: string; channel: string } | null = null;        // Other channels (status.update_channel_check)
  updateLast = "";                                                         // update.last: version, none or error:<code>
  advUpdateState = "idle";                                                 // idle, running, done, failed:<step>
  heap = 21000;
  maxBlock = 9000;
  netLow = () => false;
  snapshot: Buffer | null = null;
  send: (msg: string) => void = () => {};
  onSettingChange: (key: string) => void = () => {};

  constructor(gen: Gen, board: Board) {
    this.gen = gen;
    this.board = board;
    this.resetValues();
    this.log("sys", "info", "GBSC PRO 3.0.0-dev0 (simulator)");
    this.log("input", "info", "found RGB SCART after 1.2 s");
  }

  get caps(): number { return BOARD_CAPS[this.board]; }
  get devmode(): boolean { return Number(this.values.get("system.devmode") ?? 0) === 1; }
  uptimeMs(): number { return Date.now() - this.started; }

  resetValues(): void {
    this.values.clear();
    for (const e of this.gen.registry.settings) {
      if (e.type === "str") { this.values.set(e.key, ""); continue; }
      const v = this.gen.values[this.board][e.key];
      if (v !== undefined) this.values.set(e.key, v);
      else if ((e.caps & this.caps) === e.caps) this.values.set(e.key, e.def ?? 0);
    }
  }

  entry(key: string): RegEntry | undefined { return findSetting(this.gen.registry, key); }

  num(key: string): number { return Number(this.values.get(key) ?? 0); }

  // settingsFormat: enum label, else the number; strings as stored, secrets as ***
  format(key: string): string {
    const e = this.entry(key);
    const v = this.values.get(key);
    if (!e) return String(v ?? "");
    if (e.type === "str") return e.secret ? (v ? "***" : "") : String(v ?? "");
    if (e.type === "enum") { const o = e.options.find((x) => x.v === Number(v)); if (o) return o.n; }
    return String(v ?? 0);
  }

  // /api/settings: settingsValuesJson (available settings; dev.* only with devmode; secrets masked)
  settingsJson(): Record<string, number | string> {
    const out: Record<string, number | string> = {};
    for (const e of this.gen.registry.settings) {
      if (e.type === "str") { out[e.key] = e.secret ? (this.values.get(e.key) ? "***" : "") : String(this.values.get(e.key) ?? ""); continue; }
      if (!available(e, this.caps, this.devmode)) continue;
      out[e.key] = this.num(e.key);
    }
    return out;
  }

  // One applied write, as drainSettings: a bridged change gives no echo (the legacy command runs later);
  // everything else echoes s:<key>=<formatted>
  applyNumber(key: string, value: number): void {
    const e = this.entry(key);
    const before = this.num(key);
    this.values.set(key, value);
    this.settingsRev++;
    this.afterChange(key, before, value);
    this.onSettingChange(key);
    const bridgeFires = e?.bridge === "toggle" ? before !== value : Array.isArray(e?.bridge) ? e!.bridge!.includes(value) : false;
    if (!bridgeFires) this.send(`s:${key}=${this.format(key)}`);
  }

  applyString(key: string, value: string): void {
    this.values.set(key, value);
    this.settingsRev++;
    this.onSettingChange(key);
    this.send(`s:${key}=${this.format(key)}`);
  }

  // Runtime effects the page can observe
  private afterChange(key: string, before: number, value: number): void {
    if (key === "input.source" && value >= 1 && value <= 6 && value !== before) {
      this.runtime.activeInput = value;
      this.runtime.termination = false;
      this.event("input_changed", value, 1);
      this.log("input", "info", `manual: input ${value}`);
    }
    if (key === "output.preset") this.runtime.presetId = 0x10 | (value & 0x0f);
  }

  log(cat: string, level: "info" | "debug", text: string): number {
    const rec: LogRecord = { s: this.logSeq, t: this.uptimeMs(), c: cat, l: level, m: text.slice(0, 128) };
    this.logSeq = nextSeq(this.logSeq);
    this.logs.push(rec);
    let bytes = this.logs.reduce((n, r) => n + 9 + r.m.length, 0);
    while (bytes > 2048 && this.logs.length) bytes -= 9 + this.logs.shift()!.m.length;
    this.send("l:" + formatLogLine(rec));
    return rec.s;
  }

  // GET /api/log (logJsonBegin/logJsonFill semantics)
  logJson(since: number, boot: number): { lines: LogRecord[]; next: number; dropped: number; boot: number } {
    if (boot >= 0 && boot !== this.boot) since = -1;
    const last = this.logs.length ? this.logs[this.logs.length - 1].s : 0;
    const fromOldest = since <= 0 || (this.logs.length > 0 && seqAfter(since, last));
    const lines = fromOldest ? [...this.logs] : this.logs.filter((r) => seqAfter(r.s, since));
    let dropped = 0;
    if (!fromOldest && this.logs.length && seqAfter(this.logs[0].s, nextSeq(since))) dropped = (this.logs[0].s - since - 1) & 0xffff;
    const next = lines.length ? lines[lines.length - 1].s : since >= 0 ? since : last;
    return { lines, next, dropped, boot: this.boot };
  }

  event(type: string, a: number, b: number): number {
    const e: Event = { seq: this.eventSeq, ms: this.uptimeMs(), type, a, b };
    this.eventSeq = nextSeq(this.eventSeq);
    this.events.push(e);
    if (this.events.length > 20) this.events.shift();
    this.send(`e:${e.seq}`);
    return e.seq;
  }

  eventsJson(since: number): { boot: number; events: Event[] } {
    const last = this.events.length ? this.events[this.events.length - 1].seq : 0;
    const fromOldest = since === 0 || (this.events.length > 0 && seqAfter(since, last));
    return { boot: this.boot, events: fromOldest ? [...this.events] : this.events.filter((e) => seqAfter(e.seq, since)) };
  }

  // adv_controller.signal (ADV_CMD_SIGNAL report): the decoder only runs on S-Video and Composite; on the other
  // inputs it is powered down, as the device reported on RGB SCART
  private advSignalGen = 0;
  advSignal(): Record<string, unknown> {
    const rt = this.runtime;
    const dec = rt.activeInput === 5 || rt.activeInput === 6;
    const on = dec && !rt.noSignal;
    return { gen: ++this.advSignalGen, powered: dec, present: on, lock: on, fsc: on, freerun: dec && rt.noSignal, interlaced: on && rt.interlaced,
             colorkill: false, nonstd: false, standard: on ? (rt.advNtsc ? "NTSC" : "PAL") : null, hz: on ? (rt.srcHz10 < 550 ? 50 : 60) : null,
             vidsel: dec ? (rt.activeInput === 5 ? "0x08" : "0x00") : "0x04", encoder: null };
  }

  // After a restart: persisted settings stay; runtime state, rings and boot id start again
  reboot(fw?: string): void {
    if (fw && fw !== this.fw) { this.updateOffer = null; this.manualOffer = null; this.updateLast = ""; }   // a new image starts clean
    if (fw) this.fw = fw;
    this.started = Date.now();
    this.boot = 1 + Math.floor(Math.random() * 65534);
    this.logs = []; this.logSeq = 1; this.events = []; this.eventSeq = 1;
    this.runtime.termination = false;
    this.runtime.activeInput = this.num("input.source") || this.num("input.last") || 1;
    this.upload = { state: "idle", error: null, received: 0, total: 0 };
    this.tvRegs = { state: "idle", seg: 0, gen: 0 };
    this.wifi.scanned = false;
    this.log("sys", "info", `GBSC PRO ${this.fw} (simulator), boot ${this.boot}`);
    this.event("input_found", this.runtime.activeInput, 0);
  }

  // another channel than the running build's (a local build is on dev): the device follows it after the install
  private offerMoves(o: { channel: string } | null): number { return o && o.channel !== "beta" ? 1 : 0; }   // a local build follows Beta

  presetChar(): string {
    const low = this.runtime.presetId & 0x0f;
    return (this.runtime.presetId & 0xe0) === 0 && low >= 1 && low <= 7 ? String(low) : "0";
  }

  info(): Record<string, unknown> {
    const rt = this.runtime;
    const choice = this.num("input.source");
    const pro = this.board === "pro";
    return {
      api: 1, fw: this.fw, marker: "GBSCVER=" + this.fw + ";", build: "sim", board: this.board, caps: this.caps,
      probes: { "0x5D": { ack: pro ? 1 : 0, tries: 1 }, "0x44": { ack: pro ? 1 : 0, tries: 1 }, "0x3C": { ack: 1, tries: 1 }, rounds: 1, bus: 0 },
      heap: this.heap, max_block: this.maxBlock,
      wifi: { mode: "sta", ip: "192.168.1.50", ssid: "Home", sleep: 0, rssi: -55, channel: 6 },
      adv_controller: pro && this.advStatus !== "ok" ? { status: this.advStatus, ...(this.advUpdateState !== "idle" ? { update: this.advUpdateState } : {}) } : pro
        ? { status: "ok", version: "3.0.0", build: "446FC362", lock: rt.noSignal ? 0 : 1, standard: rt.std, uptime: Math.floor(this.uptimeMs() / 1000),
            bad_frames: 0, not_applied: false, update_available: this.advUpdate, update: this.advUpdateState, caps: [...this.advCaps],
            signal: this.advSignal() }
        : { status: "absent" },
      net: { heap_min: 18000, block_min: 8000, tcp: 1, tcp_max: 3, tw: 0, tw_max: 2, uptime: Math.floor(this.uptimeMs() / 1000),
             reset: "Power On", exc: 0, epc1: "00000000", http: 1, low: this.netLow() ? 1 : 0, tcp_refused: 0, starve_max_ms: 0, starve_at: 0 },
      clockgen: { detected: 1, hz: 81000000, preset: rt.presetId },
      input: { choice, active: rt.activeInput, last: this.num("input.last"), state: choice ? "manual" : rt.searching ? "searching" : "found",
               termination: rt.termination },
      signal: { none: rt.noSignal ? 1 : 0, std: rt.std, lines: rt.srcLines, src_hz10: rt.srcHz10, out_hz10: rt.outHz10,
                interlaced: rt.interlaced ? 1 : 0, scanlines_ok: rt.scanlinesOk ? 1 : 0, adv_ntsc: rt.advNtsc ? 1 : 0, preset: rt.presetId },
      crash: null,
      upload: { ...this.upload },
      update: { channel: "beta", last: this.updateLast, offer: { version: this.updateOffer?.version ?? "", channel: this.updateOffer?.channel ?? "", older: 0, moves: this.offerMoves(this.updateOffer) },
                manual: { version: this.manualOffer?.version ?? "", channel: this.manualOffer?.channel ?? "", older: this.manualOffer && compareVersions(this.manualOffer.version, this.fw) < 0 ? 1 : 0, moves: this.offerMoves(this.manualOffer) } },
    };
  }
}

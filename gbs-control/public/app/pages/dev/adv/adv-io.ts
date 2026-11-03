// ADV register reads and writes for the Developer tools: one dev.adv_read at a time (the device keeps one result slot;
// every section shares this queue), results from GET /api/adv/read with the announced gen; writes are the planner's
// dev.adv_i2c requests. Every step leaves a line in the console log the drawer and the console show.
import { signal } from "@preact/signals";
import { device } from "../../../store/app.ts";
import type { ReadRequest } from "../../../regs/planner.ts";

export interface LogLine { kind: "cmd" | "ok" | "err" | "sys" | "dump"; text: string }
export const advLog = signal<LogLine[]>([]);
export function logLine(kind: LogLine["kind"], text: string): void {
  advLog.value = [...advLog.value.slice(-199), { kind, text }];
  console.debug(`[adv] ${text}`);
}

export const hx = (n: number) => n.toString(16).toUpperCase().padStart(2, "0");

export class AdvReadError extends Error {
  state: string;
  constructor(state: string) { super(state); this.state = state; }
}

let queue: Promise<unknown> = Promise.resolve();

async function readNow(q: ReadRequest): Promise<number[]> {
  const res = await device.action("dev.adv_read", [q.dev, q.map, q.reg, q.count].map(hx).join(","));
  if (!res.ok) throw new AdvReadError(`HTTP ${res.status}`);
  const gen = Number((await res.json()).gen);
  for (let i = 0; i < 40; i++) {
    await new Promise((r) => setTimeout(r, 120));
    const r = await device.client.getJson<{ state: string; gen: number; values?: number[] }>("/api/adv/read");
    if (r.gen !== gen || r.state === "pending") continue;
    if (r.state !== "done") throw new AdvReadError(r.state);
    return r.values ?? [];
  }
  throw new AdvReadError("timeout");
}

// Serialised across the page: a read waits for the one before it
export function advRead(q: ReadRequest): Promise<number[]> {
  const p = queue.then(() => readNow(q));
  queue = p.catch(() => undefined);
  return p;
}

// A full action queue (409) is waited out: a long apply (Default is ~25 requests) must not stop half way
export async function advWrite(request: string): Promise<boolean> {
  logLine("cmd", `> I2C batch ${request}`);
  for (let attempt = 0; ; attempt++) {
    const res = await device.action("dev.adv_i2c", request);
    if (res.ok) return true;
    if (res.status !== 409 || attempt >= 8) { logLine("err", `not sent (HTTP ${res.status})`); return false; }
    logLine("sys", `the device's queue is full: again in ${150 * (attempt + 1)} ms`);
    await new Promise((r) => setTimeout(r, 150 * (attempt + 1)));
  }
}

export const advBusyUpdating = (): boolean => device.info.value?.adv_controller?.update === "running";
export const advCanRead = (): boolean => (device.info.value?.adv_controller?.caps ?? []).includes("reg_read");

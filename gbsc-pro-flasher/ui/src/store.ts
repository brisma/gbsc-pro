// App-wide state: the job in progress, its log, the unit host shared by the WiFi pages, confirmations
import { signal } from "@preact/signals";
import type { ProgressEvent, UnitRow } from "./api.ts";

export const busy = signal<string | null>(null);              // the running job ("flash", "update"...)
export const pct = signal<number | null>(null);
export const log = signal<string[]>([]);
export const failure = signal<string | null>(null);           // the last job's error, for the page that ran it
export const host = signal<string>("");
export const unit = signal<UnitRow | null>(null);          // the unit connected at `host`
/** A typed address: another address drops the connected unit, so nothing targets it with the old unit's data. */
export function setHost(h: string) {
  if (h !== host.value) unit.value = null;
  host.value = h;
}
export interface Ask { title: string; body: string; ok: string; resolve: (yes: boolean) => void }
export const asking = signal<Ask | null>(null);

export function note(line: string) { log.value = [...log.value.slice(-200), line]; }
export function onProgressEvent(p: ProgressEvent) {
  if (p.status) note(p.status);
  if (p.finished) note(p.finished);
  if (p.done !== null && p.total) pct.value = Math.round((p.done * 100) / p.total);
}
export function confirm(title: string, body: string, ok: string): Promise<boolean> {
  return new Promise((resolve) => { asking.value = { title, body, ok, resolve }; });
}
/** Runs one job: busy flag, fresh log and progress, the error text in the log and in `failure`. */
export async function run<T>(job: string, f: () => Promise<T>): Promise<T | null> {
  busy.value = job; pct.value = null; log.value = []; failure.value = null;
  try { return await f(); } catch (e) {
    const text = typeof e === "string" ? e : String(e);
    note(text); failure.value = text; return null;
  } finally { busy.value = null; }
}
/** Another area: the log and the error of a job that ran elsewhere go. */
export function clearJob() { log.value = []; pct.value = null; failure.value = null; }
/** run() for a job with no result: Tauri resolves a command that returns () with null, so success is "did not throw". */
export async function runOk(job: string, f: () => Promise<unknown>): Promise<boolean> {
  let ok = false;
  await run(job, async () => { await f(); ok = true; });
  return ok;
}

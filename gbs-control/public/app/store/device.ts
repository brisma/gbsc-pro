// Device state for the pages (signals): settings values with pending writes, info, live readbacks,
// profiles, console log, events and the connection. The WebSocket keeps it current: s: settings and status keys,
// l: log lines, e: events. Bridged settings never echo; they are confirmed by the settings reload that follows
// status.rev (as public/src/index.ts:206-213).
import { signal, batch } from "@preact/signals";
import type { Client } from "../api/client.ts";
import type { WsHandlers, WsConnection } from "../api/ws.ts";
import { indexRegistry, type Registry } from "../model/registry.ts";

export type Connection = "connecting" | "ok" | "reconnecting" | "offline";
export type Value = number | string;
export interface LogEntry { t: number; c: string; m: string; l?: string; s?: number }
export interface EventEntry { seq: number; ms: number; type: string; a: number; b: number }
// sentRev: the status.rev known when the device accepted the write; a reload for a newer revision already holds it.
// olds: values written before this one whose echoes may still arrive (a late echo must not undo the newer value)
export interface Pending { value: Value; prev: Value | undefined; at: number; sent: boolean; sentRev: number; olds?: Value[] }
export interface Toast { id: number; text: string; kind: "info" | "error" }
export interface UpdateState { available: string; channelCheck: string; progress: string; restore: string }
export interface ProfileEntry { id: string; name: string; region: string; fp: string; lastused: number; match?: number; modes: string[] }
// base: the output.preset value behind the active profile's Custom resolution
export interface ProfilesJson { active: string; modified: number; autoload: number; base?: number; profiles: ProfileEntry[] }
export interface WriteResult { ok: boolean; queued?: boolean; error?: string }
export interface WifiJson { mode: string; ssid: string; ip: string; hostname: string; extra_hosts: string; ap_password: string | null }

export interface StoreDeps {
  client: Client;
  connect: (url: string, h: WsHandlers) => WsConnection;
  wsUrl: string;
  registry: Registry;
  setTimeout: (fn: () => void, ms: number) => unknown;
  clearTimeout: (h: unknown) => void;
  now: () => number;
  visible: () => boolean;
  log: (msg: string) => void;
}

export const INFO_POLL_MS = 1500, INFO_POLL_HIDDEN_MS = 5000, LIVE_POLL_MS = 1000;
export const OFFLINE_AFTER_MS = 20000;
const PENDING_STALE_MS = 5000;   // a reload that still disagrees this long after the answer wins
const START_RETRY_MIN = 1000, START_RETRY_MAX = 8000;
const LOG_MAX = 400, TOAST_MS = 4000;

export type DeviceStore = ReturnType<typeof createDeviceStore>;

// Console lines: add the new ones (a line already shown with the same stamp is dropped), keep the newest max;
// replace = the device restarted
const stampKey = (e: LogEntry) => `${e.t}|${e.c}|${e.m}`;
export function mergeLog(base: LogEntry[], add: LogEntry[], replace: boolean, max: number): LogEntry[] {
  const start = replace ? [] : base;
  const seen = new Set(start.slice(-max).map(stampKey));
  const out = [...start];
  for (const e of add) { const k = stampKey(e); if (!seen.has(k)) { seen.add(k); out.push(e); } }
  return out.length > max ? out.slice(out.length - max) : out;
}

// Run fn; calls while it runs collapse into one more run afterwards
function throttled(fn: () => Promise<void>): () => Promise<void> {
  let running: Promise<void> | null = null, again = false;
  return () => {
    if (running) { again = true; return running; }
    running = (async () => {
      try { do { again = false; await fn(); } while (again); } finally { running = null; }
    })();
    return running;
  };
}

const LOG_LINE = /^\[\s*(\d+)\.(\d{3})\] \[([a-z0-9_]+)\]\s+([\s\S]*?)\n?$/;

export function createDeviceStore(d: StoreDeps) {
  const reg = indexRegistry(d.registry);
  const values = signal<Record<string, Value>>({});
  const pendingWrites = signal<Record<string, Pending>>({});
  const info = signal<any | null>(null);
  const live = signal<Record<string, any>>({});
  const profiles = signal<ProfilesJson | null>(null);
  const log = signal<LogEntry[]>([]);
  const events = signal<EventEntry[]>([]);
  const connection = signal<Connection>("connecting");
  const toasts = signal<Toast[]>([]);
  const update = signal<UpdateState>({ available: "", channelCheck: "", progress: "", restore: "" });
  const preset = signal("");
  const profileOp = signal<{ seq: number; result: string }>({ seq: 0, result: "" });
  const advStore = signal<{ seq: number; result: string }>({ seq: 0, result: "" });   // s:status.adv_store
  const wifi = signal<WifiJson | null>(null);

  let ws: WsConnection | null = null;
  let everUp = false, stopped = false;
  let lastRev = -1;
  let offlineTimer: unknown = null;
  let logNext: number | null = null, logBoot = -1;
  let evLast: number | null = null, evBoot = -1;
  let toastId = 0;
  const outbox = new Map<string, Value>();   // writes made while the device was unreachable (sent once on recovery)

  const toast = (text: string, kind: Toast["kind"] = "info") => {
    const t = { id: ++toastId, text, kind };
    toasts.value = [...toasts.value, t];
    d.setTimeout(() => { toasts.value = toasts.value.filter((x) => x.id !== t.id); }, TOAST_MS);
  };

  const same = (a: Value | undefined, b: Value | undefined) => a !== undefined && b !== undefined && String(a) === String(b);

  // ---- settings ----
  // The schema and the actions are the embedded gen/ tables: /api/schema and /api/actions are not fetched (~12 KB
  // the pages never read)
  function setValues(next: Record<string, Value>): void {
    values.value = next;
  }

  async function loadSettingsOnce(): Promise<void> {
    const revAtRequest = lastRev;   // the device applies a write in its next loop pass, before the next status.rev
    const data = await d.client.getJson<Record<string, Value>>("/api/settings");
    const pend = { ...pendingWrites.value };
    const next = { ...values.value };
    const now = d.now();
    for (const [k, v] of Object.entries(data)) {
      const p = pend[k];
      if (!p) { next[k] = v; continue; }
      if (same(p.value, v)) { delete pend[k]; next[k] = v; d.log(`${k}=${v} confirmed by the reload`); }
      else if (p.sent && revAtRequest > p.sentRev) { delete pend[k]; next[k] = v; d.log(`${k}: changed again on the device (${v})`); }
      else if (p.sent && now - p.at > PENDING_STALE_MS) { delete pend[k]; next[k] = v; d.log(`${k}: the device kept ${v}`); }
    }
    batch(() => { pendingWrites.value = pend; setValues(next); });
  }
  const reloadSettings = throttled(async () => { try { await loadSettingsOnce(); } catch (e) { d.log(`settings reload failed: ${String(e)}`); } });

  function applyEcho(key: string, raw: string): void {
    const e = reg.entry(key);
    let v: Value;
    if (e?.type === "str") v = raw;
    else if (/^-?\d+$/.test(raw)) v = Number(raw);
    else {
      const o = reg.optionValue(key, raw);
      if (o === undefined) { d.log(`${key}=${raw}: unknown label, reloading`); void reloadSettings(); return; }
      v = o;
    }
    const p = pendingWrites.value[key];
    const stale = p && !same(p.value, v) ? (p.olds ?? []).findIndex((o) => same(o, v)) : -1;
    if (p && stale >= 0) {   // the echo of an earlier write of ours: the newer value stays pending and shown
      pendingWrites.value = { ...pendingWrites.value, [key]: { ...p, olds: (p.olds ?? []).slice(stale + 1) } };
      d.log(`${key}: late echo ${v} of an earlier write, ${p.value} still pending`);
      return;
    }
    batch(() => {
      if (p) {
        const pend = { ...pendingWrites.value };
        delete pend[key];
        pendingWrites.value = pend;
        if (!same(p.value, v)) d.log(`${key}: echo ${v} replaces the pending ${p.value}`);
      }
      if (!same(values.value[key], v)) setValues({ ...values.value, [key]: v });
    });
  }

  function rollback(keys: string[]): void {
    const pend = { ...pendingWrites.value };
    const next = { ...values.value };
    for (const k of keys) {
      const p = pend[k];
      if (!p) continue;
      if (p.prev === undefined) delete next[k]; else next[k] = p.prev;
      delete pend[k];
    }
    batch(() => { pendingWrites.value = pend; setValues(next); });
  }

  async function send(pairs: [string, Value][]): Promise<WriteResult> {
    const body: Record<string, string> = {};
    for (const [k, v] of pairs) body[k] = String(v);
    let res: Response;
    try {
      res = await d.client.post("/api/settings", body);
    } catch (e) {
      d.log(`settings write not sent (${String(e)}): queued for the reconnection`);
      for (const [k, v] of pairs) outbox.set(k, v);
      return { ok: true, queued: true };
    }
    const keys = pairs.map(([k]) => k);
    if (res.ok) {
      const pend = { ...pendingWrites.value };
      for (const k of keys) if (pend[k]) pend[k] = { ...pend[k], sent: true, at: d.now(), sentRev: lastRev };
      pendingWrites.value = pend;
      return { ok: true };
    }
    let error = `HTTP ${res.status}`;
    try {
      const j = await res.json();
      if (j.errors) error = Object.entries(j.errors).map(([k, v]) => `${k}: ${v}`).join(", ");
      else if (j.error) error = String(j.error);
    } catch { /* no JSON */ }
    d.log(`settings write refused (${error}): rolled back`);
    rollback(keys);
    toast(`Not applied: ${error}`, "error");
    return { ok: false, error };
  }

  // One POST per gesture (a choice with two settings is one body); the rows show the new value at once
  async function writeSettings(pairs: [string, Value][]): Promise<WriteResult> {
    const now = d.now();
    const pend = { ...pendingWrites.value };
    const next = { ...values.value };
    for (const [k, v] of pairs) {
      const was = pend[k];
      pend[k] = { value: v, prev: was ? was.prev : values.value[k], at: now, sent: false, sentRev: lastRev,
                  olds: was ? [...(was.olds ?? []), was.value].slice(-8) : [] };
      next[k] = v;
    }
    batch(() => { pendingWrites.value = pend; setValues(next); });
    if (everUp && connection.value !== "ok") {
      for (const [k, v] of pairs) outbox.set(k, v);
      d.log(`device unreachable: ${pairs.map(([k]) => k).join(", ")} queued`);
      return { ok: true, queued: true };
    }
    return send(pairs);
  }

  async function flushOutbox(): Promise<void> {
    if (!outbox.size) return;
    const pairs = [...outbox.entries()];
    outbox.clear();
    d.log(`sending ${pairs.length} queued setting(s)`);
    await send(pairs);
  }

  // ---- other loads ----
  async function loadWifi(): Promise<void> {
    try { wifi.value = await d.client.getJson<WifiJson>("/api/wifi"); } catch (e) { d.log(`wifi: ${String(e)}`); }
  }
  // POST /api/actions/<name> with the byte parameter as "data" (comma-separated hex, as the firmware reads it)
  function action(name: string, data?: string): Promise<Response> {
    d.log(`action ${name}${data !== undefined ? " " + data : ""}`);
    return d.client.post(`/api/actions/${name}`, data === undefined ? undefined : { data });
  }

  const reloadProfiles = throttled(async () => {
    try { profiles.value = await d.client.getJson<ProfilesJson>("/api/profiles"); } catch (e) { d.log(`profiles: ${String(e)}`); }
  });

  function appendLog(add: LogEntry[], replace: boolean): void { log.value = mergeLog(log.value, add, replace, LOG_MAX); }

  async function loadLog(): Promise<void> {
    const url = logNext === null ? "/api/log" : `/api/log?since=${logNext}&boot=${logBoot}`;
    try {
      const j = await d.client.getJson<{ boot: number; next: number; lines: { s: number; t: number; c: string; l: string; m: string }[] }>(url);
      const restarted = logBoot >= 0 && j.boot !== logBoot;
      if (restarted) d.log(`device restarted (boot ${logBoot} -> ${j.boot}): console starts again`);
      appendLog(j.lines.map((r) => ({ t: r.t, c: r.c, m: r.m, l: r.l, s: r.s })), restarted || logNext === null);
      logNext = j.next; logBoot = j.boot;
    } catch (e) { d.log(`log: ${String(e)}`); }
  }

  function onLogLine(text: string): void {
    const m = LOG_LINE.exec(text);
    if (!m) { appendLog([{ t: 0, c: "?", m: text.replace(/\n$/, "") }], false); return; }
    appendLog([{ t: Number(m[1]) * 1000 + Number(m[2]), c: m[3], m: m[4] }], false);
  }

  const loadEvents = throttled(async () => {
    const url = evLast === null ? "/api/events" : `/api/events?since=${evLast}`;
    try {
      const j = await d.client.getJson<{ boot: number; events: EventEntry[] }>(url);
      const replace = evBoot >= 0 && j.boot !== evBoot;
      const list = replace || evLast === null ? j.events : [...events.value, ...j.events.filter((e) => !events.value.some((x) => x.seq === e.seq))];
      events.value = list.slice(-40);
      evBoot = j.boot;
      if (list.length) evLast = list[list.length - 1].seq;
      else if (replace) evLast = null;
    } catch (e) { d.log(`events: ${String(e)}`); }
  });

  // ---- info and live polls ----
  let infoTimer: unknown = null;
  async function pollInfo(): Promise<void> {
    infoTimer = null;
    try { info.value = await d.client.info(INFO_POLL_MS); } catch (e) { d.log(`info: ${String(e)}`); }
    if (!stopped) infoTimer = d.setTimeout(() => void pollInfo(), d.visible() ? INFO_POLL_MS : INFO_POLL_HIDDEN_MS);
  }

  let liveWanted = 0, liveTimer: unknown = null, liveRunning = false;
  async function pollLive(): Promise<void> {
    liveTimer = null;
    if (liveWanted <= 0 || stopped || liveRunning) return;
    liveRunning = true;
    try { live.value = (await d.client.getJson<{ at: number; values: Record<string, any> }>("/api/live")).values ?? {}; }
    catch (e) { d.log(`live: ${String(e)}`); }
    finally { liveRunning = false; }
    if (liveWanted > 0 && !stopped) liveTimer = d.setTimeout(() => void pollLive(), LIVE_POLL_MS);
  }
  // A row that shows a live readback calls this while it is on screen; the returned function releases it
  function wantLive(): () => void {
    liveWanted++;
    if (liveWanted === 1 && liveTimer === null) void pollLive();
    let released = false;
    return () => {
      if (released) return;
      released = true;
      liveWanted--;
      if (liveWanted === 0 && liveTimer !== null) { d.clearTimeout(liveTimer); liveTimer = null; }
    };
  }

  // ---- WebSocket ----
  function onStatus(key: string, raw: string): void {
    switch (key) {
      case "status.rev": {
        const rev = Number(raw);
        if (rev !== lastRev) { lastRev = rev; void reloadSettings(); }
        return;
      }
      case "status.profile": case "status.profile_modified": case "status.profiles_rev":
        void reloadProfiles();
        return;
      case "status.profile_op":
        profileOp.value = { seq: profileOp.value.seq + 1, result: raw };
        d.log(`profile operation: ${raw}`);
        return;
      case "status.profile_autoload": {   // "<loaded>" or "<loaded>|<profile whose unsaved changes it discarded>"
        const [loaded, discarded] = raw.split("|");   // '|' is never in a profile name
        toast(`Loaded automatically: ${loaded}${discarded ? ` · unsaved changes of ${discarded} discarded` : ""}`);
        return;
      }
      case "status.update_available": update.value = { ...update.value, available: raw }; return;
      case "status.update_channel_check": update.value = { ...update.value, channelCheck: raw }; return;
      case "status.update": update.value = { ...update.value, progress: raw }; return;
      case "status.restore": update.value = { ...update.value, restore: raw }; return;
      case "status.preset": preset.value = raw; return;
      case "status.adv_store":
        advStore.value = { seq: advStore.value.seq + 1, result: raw };
        d.log(`adv store: ${raw}`);
        return;
      default: return;
    }
  }

  function onMessage(m: string): void {
    if (m.startsWith("s:")) {
      const body = m.slice(2), eq = body.indexOf("=");
      if (eq < 0) return;
      const key = body.slice(0, eq), raw = body.slice(eq + 1);
      if (key.startsWith("status.")) onStatus(key, raw); else applyEcho(key, raw);
    } else if (m.startsWith("l:")) onLogLine(m.slice(2));
    else if (m.startsWith("e:")) void loadEvents();
  }

  function recover(): void {
    d.log("connection back: catching up");
    void reloadSettings();
    void loadLog();
    void loadEvents();
    void reloadProfiles();
    void flushOutbox();
  }

  function onWsState(s: "connecting" | "open" | "down"): void {
    if (s === "open") {
      if (offlineTimer !== null) { d.clearTimeout(offlineTimer); offlineTimer = null; }
      const wasDown = everUp && connection.value !== "ok";
      everUp = true;
      connection.value = "ok";
      if (wasDown) recover();
    } else if (s === "down" && everUp && connection.value === "ok") {
      connection.value = "reconnecting";
      offlineTimer = d.setTimeout(() => { offlineTimer = null; if (connection.value === "reconnecting") connection.value = "offline"; }, OFFLINE_AFTER_MS);
    }
  }

  async function start(): Promise<void> {
    stopped = false;
    ws = d.connect(d.wsUrl, { message: onMessage, state: onWsState });
    await Promise.allSettled([
      loadSettingsOnce().catch((e) => d.log(`settings: ${String(e)}`)), pollInfo(),
      reloadProfiles(), loadLog(), loadEvents(), loadWifi(),
    ]);
    retryStart(START_RETRY_MIN);
  }

  // A busy device can refuse the start-up fan-out longer than the client's retries: what did not arrive is asked
  // again (1 s, doubling to 8 s) until everything is in (info polls by itself)
  function retryStart(delay: number): void {
    const missing = () => [
      Object.keys(values.value).length === 0 ? "settings" : "", profiles.value === null ? "profiles" : "",
      wifi.value === null ? "wifi" : "", logNext === null ? "log" : "",
    ].filter(Boolean);
    if (stopped || !missing().length) return;
    d.setTimeout(async () => {
      const m = missing();
      if (stopped || !m.length) return;
      d.log(`start-up: ${m.join(", ")} not loaded yet, asking again`);
      await Promise.allSettled([
        m.includes("settings") ? loadSettingsOnce().catch((e) => d.log(`settings: ${String(e)}`)) : null,
        m.includes("profiles") ? reloadProfiles() : null, m.includes("wifi") ? loadWifi() : null, m.includes("log") ? loadLog() : null,
      ]);
      retryStart(Math.min(START_RETRY_MAX, delay * 2));
    }, delay);
  }

  function stop(): void {
    stopped = true;
    ws?.close();
    for (const t of [infoTimer, liveTimer, offlineTimer]) if (t !== null) d.clearTimeout(t);
  }

  return {
    values, pendingWrites, info, live, profiles, log, events, connection, toasts, update, preset,
    profileOp, advStore, wifi, registry: reg, client: d.client,
    start, stop, writeSettings, reloadSettings, reloadProfiles, loadWifi, wantLive, onMessage, toast, action,
  };
}

// Device requests (ported from public/src/index.ts:14-110). The ESP8266 has ~16 KB of free heap: 4+ parallel
// requests can crash it, so it refuses extra ones at once with 503 + X-GBSC-Busy. Here: at most 2 requests running
// (the rest queue), a busy 503 is retried with jitter (the request never ran), a GET is sent again after a reset
// connection, and nothing else starts while a firmware upload runs.

export interface ClientDeps {
  fetch: (input: string, init?: RequestInit) => Promise<Response>;
  sleep: (ms: number) => Promise<void>;
  random: () => number;
  now: () => number;
  setTimeout: (fn: () => void, ms: number) => unknown;
  clearTimeout: (h: unknown) => void;
  log: (msg: string) => void;
}

export interface Client {
  get(path: string): Promise<Response>;
  getJson<T = any>(path: string): Promise<T>;                       // throws on a non-2xx answer
  post(path: string, form?: Record<string, string> | URLSearchParams | FormData): Promise<Response>;
  exclusive(path: string, body: BodyInit): Promise<Response>;        // POST alone (uploads), 180 s
  info(maxAgeMs?: number): Promise<any>;                            // /api/info shared by callers within maxAgeMs
  stats(): { inFlight: number; queued: number; exclusive: boolean };
}

export const MAX_IN_FLIGHT = 2;
export const TIMEOUT_MS = 15000;
export const UPLOAD_TIMEOUT_MS = 180000;
const BUSY_RETRIES = 5;
const NET_RETRIES = 2;
const CONFLICT_RETRIES = 2;   // 409 {"error":"busy"} on settings/actions: the device queue is full for a moment

const defaults = (): ClientDeps => ({
  fetch: (i, n) => fetch(i, n),
  sleep: (ms) => new Promise((r) => setTimeout(r, ms)),
  random: Math.random,
  now: Date.now,
  setTimeout: (fn, ms) => setTimeout(fn, ms),
  clearTimeout: (h) => clearTimeout(h as number),
  log: (m) => console.debug("[api] " + m),
});

const isBusy = (r: Response) => r.status === 503 && r.headers.get("X-GBSC-Busy") !== null;

export function createClient(over: Partial<ClientDeps> = {}): Client {
  const d: ClientDeps = { ...defaults(), ...over };
  let inFlight = 0;
  let exclusiveOn = false;
  const waiters: (() => void)[] = [];
  const idleWaiters: (() => void)[] = [];   // an upload waiting for the running requests

  const pump = () => {
    if (inFlight === 0) while (idleWaiters.length) idleWaiters.shift()!();
    while (!exclusiveOn && inFlight < MAX_IN_FLIGHT && waiters.length) { inFlight++; waiters.shift()!(); }
  };
  const slot = () => new Promise<void>((resolve) => { waiters.push(resolve); pump(); });

  // One request; the body is read before the slot is released (the connection stays open until then)
  async function once(path: string, init: RequestInit, timeoutMs: number): Promise<Response> {
    const ctl = typeof AbortController !== "undefined" ? new AbortController() : null;
    const h = d.setTimeout(() => ctl?.abort(), timeoutMs);
    try {
      const res = await d.fetch(path, ctl ? { ...init, signal: ctl.signal } : init);
      const body = await res.arrayBuffer();
      const empty = res.status === 204 || res.status === 205 || res.status === 304;
      return new Response(empty ? null : body, { status: res.status, statusText: res.statusText, headers: res.headers });
    } finally {
      d.clearTimeout(h);
    }
  }

  async function request(path: string, init: RequestInit = {}): Promise<Response> {
    const idempotent = !init.method || init.method === "GET";
    for (let attempt = 0; ; attempt++) {
      await slot();
      let res: Response | null = null;
      try {
        res = await once(path, init, TIMEOUT_MS);
      } catch (e) {
        if (!idempotent || attempt >= NET_RETRIES) { d.log(`${init.method ?? "GET"} ${path} failed: ${String(e)}`); throw e; }
      } finally {
        inFlight--;
        pump();
      }
      if (!res) { d.log(`GET ${path}: connection reset, again`); await d.sleep(400 + d.random() * 800); continue; }
      if (!isBusy(res) || attempt >= BUSY_RETRIES) return res;
      const wait = 400 + d.random() * 800 * (attempt + 1);
      d.log(`${path}: busy (${res.headers.get("X-GBSC-Busy")}), retry ${attempt + 1} in ${Math.round(wait)} ms`);
      await d.sleep(wait);
    }
  }

  const conflictRetried = (path: string) => path === "/api/settings" || path.startsWith("/api/actions/");

  async function post(path: string, form?: Record<string, string> | URLSearchParams | FormData): Promise<Response> {
    const body = form === undefined ? undefined : form instanceof URLSearchParams || form instanceof FormData ? form : new URLSearchParams(form);
    for (let attempt = 0; ; attempt++) {
      const res = await request(path, { method: "POST", body, headers: { "X-GBSC": "1" } });
      if (res.status !== 409 || !conflictRetried(path) || attempt >= CONFLICT_RETRIES) return res;
      let busy = false;
      try { busy = (await res.clone().json()).error === "busy"; } catch { /* not JSON */ }
      if (!busy) return res;
      d.log(`${path}: device queue full, retry ${attempt + 1}`);
      await d.sleep(300);
    }
  }

  // Runs alone: waits for the running requests, holds the queue. A busy answer comes only after the whole body was
  // sent, so only "load" is retried (twice); "memory" and "update" return at once.
  async function exclusive(path: string, body: BodyInit): Promise<Response> {
    if (exclusiveOn) return new Response(null, { status: 409 });
    exclusiveOn = true;
    try {
      if (inFlight > 0) await new Promise<void>((r) => idleWaiters.push(r));
      for (let attempt = 0; ; attempt++) {
        const res = await once(path, { method: "POST", body, headers: { "X-GBSC": "1" } }, UPLOAD_TIMEOUT_MS);
        if (!isBusy(res) || res.headers.get("X-GBSC-Busy") !== "load" || attempt >= 2) return res;
        d.log(`${path}: busy (load) before the upload, retry ${attempt + 1}`);
        await d.sleep(800 + d.random() * 800);
      }
    } finally {
      exclusiveOn = false;
      pump();
    }
  }

  let infoPromise: Promise<any> | null = null;
  let infoAt = -Infinity;
  function info(maxAgeMs = 1500): Promise<any> {
    const now = d.now();
    if (!infoPromise || now - infoAt > maxAgeMs) {
      infoAt = now;
      const p = getJson("/api/info");
      p.catch(() => { if (infoPromise === p) infoPromise = null; });
      infoPromise = p;
    }
    return infoPromise;
  }

  async function getJson<T = any>(path: string): Promise<T> {
    const r = await request(path);
    if (!r.ok) throw new Error(`${path}: HTTP ${r.status}`);
    return (await r.json()) as T;
  }

  return {
    get: (path) => request(path),
    getJson,
    post,
    exclusive,
    info,
    stats: () => ({ inFlight, queued: waiters.length, exclusive: exclusiveOn }),
  };
}

// Fakes for the web app data layer tests: fetch with deferred answers, sleeps that resolve at once, a WebSocket
// connector driven by the test
import type { ClientDeps } from "../../app/api/client.ts";
import type { WsHandlers } from "../../app/api/ws.ts";

export interface Call { url: string; method: string; body: string; headers: Record<string, string> }
type Answer = Response | (() => Response) | Error;

export const json = (status: number, body: unknown, headers: Record<string, string> = {}) =>
  new Response(JSON.stringify(body), { status, headers: { "content-type": "application/json", ...headers } });
export const busy = (kind = "load") => new Response("{}", { status: 503, headers: { "X-GBSC-Busy": kind, "Retry-After": "1" } });

// Every fetch waits until the test answers it (answer(i, r)) unless an auto rule matches
export class FakeFetch {
  calls: Call[] = [];
  private waiting: { call: Call; resolve: (r: Response) => void; reject: (e: unknown) => void }[] = [];
  auto: ((c: Call) => Answer | undefined)[] = [];

  fetch = (url: string, init: RequestInit = {}): Promise<Response> => {
    const headers: Record<string, string> = {};
    new Headers(init.headers ?? {}).forEach((v, k) => { headers[k] = v; });
    const body = init.body === undefined || init.body === null ? "" : String(init.body);
    const call = { url, method: init.method ?? "GET", body, headers };
    this.calls.push(call);
    for (const rule of this.auto) {
      const a = rule(call);
      if (a !== undefined) return a instanceof Error ? Promise.reject(a) : Promise.resolve(typeof a === "function" ? a() : a);
    }
    return new Promise((resolve, reject) => this.waiting.push({ call, resolve, reject }));
  };

  pending(): Call[] { return this.waiting.map((w) => w.call); }
  answer(match: string, r: Response | Error): void {
    const i = this.waiting.findIndex((w) => w.call.url.startsWith(match));
    if (i < 0) throw new Error("no pending request for " + match + "; pending: " + this.pending().map((c) => c.url).join(", "));
    const [w] = this.waiting.splice(i, 1);
    if (r instanceof Error) w.reject(r); else w.resolve(r);
  }
  count(match: string, method?: string): number {
    return this.calls.filter((c) => c.url.startsWith(match) && (!method || c.method === method)).length;
  }
}

export const tick = () => new Promise((r) => setImmediate(r));
export async function settle(n = 10): Promise<void> { for (let i = 0; i < n; i++) await tick(); }

export function clientDeps(f: FakeFetch, sleeps: number[] = []): Partial<ClientDeps> {
  return {
    fetch: f.fetch as ClientDeps["fetch"],
    sleep: (ms: number) => { sleeps.push(ms); return Promise.resolve(); },
    random: () => 0.5,
    log: () => {},
  };
}

// Timers the test fires by hand
export class FakeTimers {
  now = 0;
  private q: { at: number; fn: () => void; id: number }[] = [];
  private nextId = 1;
  delays: number[] = [];
  setTimeout = (fn: () => void, ms: number): number => {
    const id = this.nextId++;
    this.delays.push(ms);
    this.q.push({ at: this.now + ms, fn, id });
    return id;
  };
  clearTimeout = (id: unknown): void => { this.q = this.q.filter((t) => t.id !== id); };
  advance(ms: number): void {
    const end = this.now + ms;
    for (;;) {
      this.q.sort((a, b) => a.at - b.at);
      const t = this.q[0];
      if (!t || t.at > end) break;
      this.q.shift();
      this.now = t.at;
      t.fn();
    }
    this.now = end;
  }
  size(): number { return this.q.length; }
}

// A WebSocket the test opens, feeds and closes
export class FakeSocket {
  onopen: (() => void) | null = null;
  onmessage: ((e: { data: unknown }) => void) | null = null;
  onclose: (() => void) | null = null;
  onerror: (() => void) | null = null;
  closed = false;
  url: string;
  protocols: string[];
  constructor(url: string, protocols: string[]) { this.url = url; this.protocols = protocols; }
  silentClose = false;   // a dead WiFi link: close() never completes the handshake, onclose comes late or never
  close(): void { if (this.closed) return; this.closed = true; if (!this.silentClose) this.onclose?.(); }
  open(): void { this.onopen?.(); }
  recv(m: string): void { this.onmessage?.({ data: m }); }
}

// Store-side connector: records the handlers so the test plays the device
export class FakeConnector {
  handlers: WsHandlers | null = null;
  closed = 0;
  connect = (_url: string, h: WsHandlers) => { this.handlers = h; return { close: () => { this.closed++; } }; };
  up(): void { this.handlers!.state("open"); }
  down(): void { this.handlers!.state("down"); }
  msg(m: string): void { this.handlers!.message(m); }
}

// Device WebSocket (ws://host:81/, subprotocol "arduino"): s: settings and status, l: log lines, e: events. The
// device sends status.rev about every second, so 2.7 s of silence means a dead link (a WiFi blip leaves the socket
// open, and a dead link delays the browser's close event by 20-60 s: the socket is dropped at once); a socket that does
// not open within 7 s is dropped too; reconnect after 1 s, doubling to 8 s, back to 1 s once a message arrived.

export interface WsLike {
  onopen: (() => void) | null;
  onmessage: ((e: { data: unknown }) => void) | null;
  onclose: (() => void) | null;
  onerror: (() => void) | null;
  close(): void;
}
export interface WsDeps {
  create: (url: string, protocols: string[]) => WsLike;
  setTimeout: (fn: () => void, ms: number) => unknown;
  clearTimeout: (h: unknown) => void;
  log: (msg: string) => void;
}
export type WsState = "connecting" | "open" | "down";
export interface WsHandlers { message(m: string): void; state(s: WsState): void }
export interface WsConnection { close(): void }

export const LIVENESS_MS = 2700;
export const CONNECT_MS = 7000;   // opening: the device may be busy serving the page (~25 KB/s); the old page's value
const BACKOFF_MIN = 1000, BACKOFF_MAX = 8000;

const defaults = (): WsDeps => ({
  create: (url, protocols) => new WebSocket(url, protocols) as unknown as WsLike,
  setTimeout: (fn, ms) => setTimeout(fn, ms),
  clearTimeout: (h) => clearTimeout(h as number),
  log: (m) => console.debug("[ws] " + m),
});

export function connectWs(url: string, h: WsHandlers, over: Partial<WsDeps> = {}): WsConnection {
  const d: WsDeps = { ...defaults(), ...over };
  let sock: WsLike | null = null;
  let stopped = false;
  let backoff = BACKOFF_MIN;
  let liveness: unknown = null;
  let retry: unknown = null;

  const clearLiveness = () => { if (liveness !== null) { d.clearTimeout(liveness); liveness = null; } };
  const armLiveness = (s: WsLike, ms = LIVENESS_MS) => {
    clearLiveness();
    liveness = d.setTimeout(() => {
      liveness = null;
      if (s !== sock) return;
      // a dead link: the browser fires close only after its closing handshake times out (20-60 s), so the socket is
      // dropped here and the page reconnects without waiting for it
      d.log(`no message for ${ms} ms: dropping the socket`);
      s.onopen = null; s.onmessage = null; s.onclose = null; s.onerror = null;
      down(s);
      s.close();
    }, ms);
  };

  const down = (s: WsLike) => {
    if (s !== sock) return;
    sock = null;
    clearLiveness();
    h.state("down");
    if (stopped) return;
    d.log(`closed, reconnecting in ${backoff} ms`);
    retry = d.setTimeout(() => { retry = null; open(); }, backoff);
    backoff = Math.min(BACKOFF_MAX, backoff * 2);
  };

  function open(): void {
    if (stopped) return;
    h.state("connecting");
    let s: WsLike;
    try {
      s = d.create(url, ["arduino"]);
    } catch (e) {
      d.log(`cannot open: ${String(e)}`);
      const dummy = { onopen: null, onmessage: null, onclose: null, onerror: null, close() {} } as WsLike;
      sock = dummy;
      down(dummy);
      return;
    }
    sock = s;
    armLiveness(s, CONNECT_MS);   // a socket that never opens (stuck connecting) is dropped too
    s.onopen = () => { if (s !== sock) return; d.log("open"); h.state("open"); armLiveness(s); };
    s.onmessage = (e) => {
      if (s !== sock) return;
      backoff = BACKOFF_MIN;
      armLiveness(s);
      h.message(String(e.data));
    };
    s.onclose = () => down(s);
    s.onerror = () => { /* a close follows */ };
  }

  open();
  return {
    close() {
      stopped = true;
      if (retry !== null) d.clearTimeout(retry);
      clearLiveness();
      const s = sock;
      sock = null;
      s?.close();
    },
  };
}

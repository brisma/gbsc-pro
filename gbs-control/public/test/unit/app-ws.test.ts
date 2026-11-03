// WebSocket of the web app: subprotocol, liveness timeout, reconnect backoff
import { test } from "node:test";
import assert from "node:assert/strict";
import { connectWs, LIVENESS_MS, CONNECT_MS } from "../../app/api/ws.ts";
import { FakeSocket, FakeTimers } from "./app-helpers.ts";

function rig() {
  const t = new FakeTimers();
  const sockets: FakeSocket[] = [];
  const states: string[] = [];
  const msgs: string[] = [];
  const conn = connectWs("ws://gbs:81/", { message: (m) => msgs.push(m), state: (s) => states.push(s) }, {
    create: (url, protocols) => { const s = new FakeSocket(url, protocols); sockets.push(s); return s; },
    setTimeout: t.setTimeout, clearTimeout: t.clearTimeout, log: () => {},
  });
  return { t, sockets, states, msgs, conn };
}

test("ws uses the arduino subprotocol", () => {
  const r = rig();
  assert.equal(r.sockets.length, 1);
  assert.equal(r.sockets[0].url, "ws://gbs:81/");
  assert.deepEqual(r.sockets[0].protocols, ["arduino"]);
  r.sockets[0].open();
  r.sockets[0].recv("s:status.rev=3");
  assert.deepEqual(r.msgs, ["s:status.rev=3"]);
  assert.deepEqual(r.states, ["connecting", "open"]);
});

test("ws liveness timeout", () => {
  const r = rig();
  assert.equal(LIVENESS_MS, 2700);
  r.sockets[0].open();
  r.t.advance(2000); r.sockets[0].recv("s:status.rev=1");   // a message restarts the timer
  r.t.advance(2600);
  assert.equal(r.sockets[0].closed, false);
  r.t.advance(200);
  assert.equal(r.sockets[0].closed, true);                  // no message for 2.7 s: the link is dead
  assert.equal(r.states.at(-1), "down");
  r.t.advance(1000);
  assert.equal(r.sockets.length, 2);                        // and a new socket after 1 s
});

test("ws reconnects with backoff 1,2,4,8 s", () => {
  const r = rig();
  r.sockets[0].close();
  for (let i = 1; i <= 5; i++) {
    const before = r.sockets.length;
    const want = [1000, 2000, 4000, 8000, 8000][i - 1];
    r.t.advance(want - 1);
    assert.equal(r.sockets.length, before, `attempt ${i} not before ${want} ms`);
    r.t.advance(1);
    assert.equal(r.sockets.length, before + 1, `attempt ${i} at ${want} ms`);
    r.sockets.at(-1)!.close();
  }
  const s = r.sockets.at(-1)!;
  r.t.advance(8000);
  r.sockets.at(-1)!.open(); r.sockets.at(-1)!.recv("s:status.rev=1");   // a working link resets the backoff
  r.sockets.at(-1)!.close();
  const n = r.sockets.length;
  r.t.advance(1000);
  assert.equal(r.sockets.length, n + 1);
  assert.ok(s.closed);
});

test("ws close stops reconnecting", () => {
  const r = rig();
  r.conn.close();
  r.t.advance(20000);
  assert.equal(r.sockets.length, 1);
});

// On a dead link the browser fires close only after its closing-handshake timeout (Firefox 20 s,
// Chromium ~60 s): the banner and the next socket must not wait for it
test("ws liveness on a silent link: down and a new socket without waiting for onclose", () => {
  const r = rig();
  r.sockets[0].silentClose = true;
  r.sockets[0].open();
  r.t.advance(LIVENESS_MS + 10);
  assert.equal(r.sockets[0].closed, true);
  assert.equal(r.states.at(-1), "down");                   // the banner at once
  r.t.advance(1000);
  assert.equal(r.sockets.length, 2);                        // the next try after the backoff
  r.sockets[0].onclose?.();                                 // the late close of the old socket changes nothing
  assert.equal(r.states.at(-1), "connecting");
});

test("ws a socket stuck connecting is dropped", () => {
  const r = rig();
  r.sockets[0].silentClose = true;                          // never opens, never closes
  r.t.advance(CONNECT_MS - 100);
  assert.equal(r.sockets[0].closed, false);                 // a busy device gets its 7 s to open
  r.t.advance(200);
  assert.equal(r.sockets[0].closed, true);
  assert.equal(r.states.at(-1), "down");
  r.t.advance(1000);
  assert.equal(r.sockets.length, 2);
});

// Device store of the web app: WebSocket status handling, pending writes, reloads, recovery
import { test } from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { join } from "node:path";
import { createClient } from "../../app/api/client.ts";
import { createDeviceStore } from "../../app/store/device.ts";
import { FakeFetch, FakeConnector, FakeTimers, json, clientDeps, settle } from "./app-helpers.ts";
import { publicDir } from "./sim-helpers.ts";

const registry = JSON.parse(readFileSync(join(publicDir, "gen", "registry.json"), "utf8"));
const values = JSON.parse(readFileSync(join(publicDir, "gen", "values.pro.json"), "utf8"));

function rig() {
  const f = new FakeFetch();
  const ws = new FakeConnector();
  const t = new FakeTimers();
  let settings: Record<string, unknown> = { ...values };
  const hold = { settings: false };   // true: GET /api/settings waits for f.answer
  f.auto.push((c) => {
    const path = c.url.split("?")[0];
    if (c.method === "GET" && path === "/api/settings") return hold.settings ? undefined : json(200, settings);
    if (path === "/api/schema" || path === "/api/actions") return json(200, []);
    if (path === "/api/info") return json(200, { fw: "3.0.0", board: "pro" });
    if (path === "/api/wifi") return json(200, { mode: "STA", ssid: "Home", ip: "192.168.1.50", hostname: "gbscontrol.local", extra_hosts: "", ap_password: null });
    if (path === "/api/profiles") return json(200, { active: "", modified: 0, autoload: 0, profiles: [] });
    if (path === "/api/log") return json(200, { boot: 77, next: 3, dropped: 0, lines: [{ s: 2, t: 1500, c: "sys", l: "info", m: "booted" }, { s: 3, t: 1600, c: "gbs", l: "info", m: "preset applied" }] });
    if (path === "/api/events") return json(200, { boot: 77, events: [{ seq: 1, ms: 900, type: "input_found", a: 1, b: 0 }] });
    return undefined;
  });
  const store = createDeviceStore({
    client: createClient(clientDeps(f)), connect: ws.connect, wsUrl: "ws://x/", registry,
    setTimeout: t.setTimeout, clearTimeout: t.clearTimeout, now: () => t.now, visible: () => true, log: () => {},
  });
  return { f, ws, t, store, hold, set: (s: Record<string, unknown>) => { settings = s; } };
}

async function started() {
  const r = rig();
  await r.store.start();
  r.ws.up();
  await settle();
  return r;
}

test("start loads settings, info, profiles, log, events and wifi (schema and actions are embedded, never fetched)", async () => {
  const r = await started();
  for (const p of ["/api/settings", "/api/info", "/api/profiles", "/api/log", "/api/events", "/api/wifi"])
    assert.equal(r.f.count(p), 1, p);
  assert.equal(r.f.count("/api/schema"), 0);   // ~10 KB the app never read: it uses gen/registry.json
  assert.equal(r.f.count("/api/actions"), 0);
  assert.equal(r.store.values.value["adv.brightness"], values["adv.brightness"]);
  assert.equal(r.store.connection.value, "ok");
  assert.equal(r.store.log.value.length, 2);
});

test("enum echo label mapped to the value", async () => {
  const r = await started();
  r.ws.msg("s:input.source=S-Video");
  assert.equal(r.store.values.value["input.source"], 5);
  r.ws.msg("s:adv.brightness=61");
  assert.equal(r.store.values.value["adv.brightness"], 61);
});

test("bridged write confirmed after the rev reload", async () => {
  const r = await started();
  const old = r.store.values.value["output.preset"];
  const w = r.store.writeSettings([["output.preset", 3]]);
  assert.equal(r.store.values.value["output.preset"], 3);                 // shown at once
  assert.ok(r.store.pendingWrites.value["output.preset"]);
  r.f.auto.unshift((c) => (c.method === "POST" ? json(200, { ok: 1 }) : undefined));
  assert.deepEqual(await w, { ok: true });
  await settle();
  assert.ok(r.store.pendingWrites.value["output.preset"], "no echo for a bridged key: still pending");
  r.set({ ...values, "output.preset": 3 });
  r.ws.msg("s:status.rev=9");
  await settle();
  assert.equal(r.store.pendingWrites.value["output.preset"], undefined);
  assert.equal(r.store.values.value["output.preset"], 3);
  assert.notEqual(old, 3);
});

test("rev reloads are throttled", async () => {
  const r = await started();
  const before = r.f.count("/api/settings");
  r.hold.settings = true;
  r.ws.msg("s:status.rev=10"); r.ws.msg("s:status.rev=11"); r.ws.msg("s:status.rev=12"); r.ws.msg("s:status.rev=13");
  await settle();
  assert.equal(r.f.count("/api/settings") - before, 1);                 // one in flight
  r.f.answer("/api/settings", json(200, values));
  await settle();
  assert.equal(r.f.count("/api/settings") - before, 2);                 // one queued after it, not four
  r.f.answer("/api/settings", json(200, values));
  await settle();
  assert.equal(r.f.count("/api/settings") - before, 2);
  r.ws.msg("s:status.rev=13");                                          // the same revision: nothing to do
  await settle();
  assert.equal(r.f.count("/api/settings") - before, 2);
});

test("profile status reloads the list", async () => {
  const r = await started();
  r.ws.msg("s:status.profiles_rev=5");
  await settle();
  assert.equal(r.f.count("/api/profiles"), 2);
  r.ws.msg("s:status.profile_modified=1");
  await settle();
  assert.equal(r.f.count("/api/profiles"), 3);
  r.ws.msg("s:status.profile_op=error:limit");
  assert.equal(r.store.profileOp.value.result, "error:limit");
  r.ws.msg("s:status.profile_autoload=Saturn");
  assert.equal(r.store.toasts.value.at(-1)!.text, "Loaded automatically: Saturn");
  r.ws.msg("s:status.profile_autoload=MD NTSC-U|N64 PAL");   // over unsaved changes of another profile
  assert.equal(r.store.toasts.value.at(-1)!.text, "Loaded automatically: MD NTSC-U · unsaved changes of N64 PAL discarded");
});

test("devmode change fetches nothing (schema and actions are embedded)", async () => {
  const r = await started();
  r.ws.msg("s:system.devmode=1");
  await settle();
  assert.equal(r.f.count("/api/schema"), 0);
  assert.equal(r.f.count("/api/actions"), 0);
});

// A busy start-up longer than the client's retries left the page empty (and behind the loader)
test("start-up loads that fail are tried again until the device answers", async () => {
  const r = rig();
  const busy: Record<string, number> = { "/api/settings": 7, "/api/profiles": 7, "/api/wifi": 7, "/api/log": 7 };   // > 6 tries each
  r.f.auto.unshift((c) => {
    const path = c.url.split("?")[0];
    if (busy[path] > 0) {
      busy[path]--;
      return new Response(null, { status: 503, headers: { "X-GBSC-Busy": "load", "Retry-After": "1" } });
    }
    return undefined;
  });
  await r.store.start();
  r.ws.up();
  await settle();
  assert.deepEqual(r.store.values.value, {});
  for (let i = 0; i < 6 && Object.keys(r.store.values.value).length === 0; i++) { r.t.advance(8000); await settle(); }
  assert.equal(r.store.values.value["adv.brightness"], values["adv.brightness"]);
  assert.ok(r.store.profiles.value !== null, "profiles loaded");
  assert.ok(r.store.wifi.value !== null, "wifi loaded");
  assert.equal(r.store.log.value.length, 2);
  const n = r.f.count("/api/settings");
  r.t.advance(30000); await settle();
  assert.equal(r.f.count("/api/settings"), n, "no more tries once everything came");
});

test("pending write rolls back on 400", async () => {
  const r = await started();
  const old = r.store.values.value["adv.brightness"];
  r.f.auto.unshift((c) => (c.method === "POST" ? json(400, { ok: 0, errors: { "adv.brightness": "range" } }) : undefined));
  const res = await r.store.writeSettings([["adv.brightness", 300]]);
  assert.equal(res.ok, false);
  assert.match(res.error!, /adv.brightness/);
  assert.equal(r.store.values.value["adv.brightness"], old);
  assert.equal(r.store.pendingWrites.value["adv.brightness"], undefined);
});

test("echo confirms a pending write", async () => {
  const r = await started();
  r.f.auto.unshift((c) => (c.method === "POST" ? json(200, { ok: 1 }) : undefined));
  await r.store.writeSettings([["adv.brightness", 70]]);
  assert.ok(r.store.pendingWrites.value["adv.brightness"]);
  r.ws.msg("s:adv.brightness=70");
  assert.equal(r.store.pendingWrites.value["adv.brightness"], undefined);
});

test("one post per gesture with every pair", async () => {
  const r = await started();
  r.f.auto.unshift((c) => (c.method === "POST" ? json(200, { ok: 1 }) : undefined));
  await r.store.writeSettings([["picture.scanlines", 1], ["picture.scanline_strength", 48]]);
  const posts = r.f.calls.filter((c) => c.method === "POST");
  assert.equal(posts.length, 1);
  assert.equal(posts[0].body, "picture.scanlines=1&picture.scanline_strength=48");
});

test("writes queued during a drop are sent once after recovery", async () => {
  const r = await started();
  r.f.auto.unshift((c) => (c.method === "POST" ? json(200, { ok: 1 }) : undefined));
  r.ws.down();
  assert.equal(r.store.connection.value, "reconnecting");
  void r.store.writeSettings([["adv.brightness", 41]]);
  void r.store.writeSettings([["adv.brightness", 42], ["adv.contrast", 50]]);
  await settle();
  assert.equal(r.f.count("/api/settings", "POST"), 0);
  assert.equal(r.store.values.value["adv.brightness"], 42);
  r.ws.up();
  await settle(30);
  const posts = r.f.calls.filter((c) => c.method === "POST");
  assert.equal(posts.length, 1);
  assert.equal(posts[0].body, "adv.brightness=42&adv.contrast=50");
  r.ws.down(); r.ws.up();
  await settle(30);
  assert.equal(r.f.calls.filter((c) => c.method === "POST").length, 1);   // sent once
});

test("log start uses since and boot", async () => {
  const r = await started();
  assert.equal(r.f.calls.find((c) => c.url.startsWith("/api/log"))!.url, "/api/log");
  r.ws.msg("l:[     1.600] [gbs]      preset applied\n");             // already shown by stamp: dropped
  assert.equal(r.store.log.value.length, 2);
  r.ws.msg("l:[     2.000] [input]    found RGB SCART\n");
  assert.equal(r.store.log.value.length, 3);
  assert.deepEqual({ ...r.store.log.value[2] }, { t: 2000, c: "input", m: "found RGB SCART" });
  r.ws.down(); r.ws.up();
  await settle(30);
  assert.ok(r.f.calls.some((c) => c.url === "/api/log?since=3&boot=77"), r.f.calls.map((c) => c.url).join(" "));
  assert.ok(r.f.calls.some((c) => c.url === "/api/events?since=1"));
  assert.equal(r.f.count("/api/settings", "GET"), 2);
  assert.equal(r.f.count("/api/profiles"), 2);
});

test("events refetch on e:", async () => {
  const r = await started();
  r.ws.msg("e:2");
  await settle();
  assert.ok(r.f.calls.some((c) => c.url === "/api/events?since=1"));
});

test("connection goes offline after 20 s down", async () => {
  const r = await started();
  r.ws.down();
  assert.equal(r.store.connection.value, "reconnecting");
  r.t.advance(20000);
  assert.equal(r.store.connection.value, "offline");
  r.ws.up();
  assert.equal(r.store.connection.value, "ok");
});

test("update and preset status keys", async () => {
  const r = await started();
  r.ws.msg("s:status.update=35");
  assert.equal(r.store.update.value.progress, "35");
  r.ws.msg("s:status.update_available=3.0.1");
  assert.equal(r.store.update.value.available, "3.0.1");
  r.ws.msg("s:status.restore=ok");
  assert.equal(r.store.update.value.restore, "ok");
  r.ws.msg("s:status.preset=3");
  assert.equal(r.store.preset.value, "3");
});

test("live polls only while wanted", async () => {
  const r = await started();
  r.f.auto.unshift((c) => (c.url === "/api/live" ? json(200, { at: 1, values: { adc_gain: 9 } }) : undefined));
  const release = r.store.wantLive();
  await settle();
  assert.equal(r.store.live.value.adc_gain, 9);
  const n = r.f.count("/api/live");
  r.t.advance(1000); await settle();
  assert.equal(r.f.count("/api/live"), n + 1);
  release();
  r.t.advance(5000); await settle();
  assert.equal(r.f.count("/api/live"), n + 1);
});

test("action posts its data", async () => {
  const r = await started();
  r.f.auto.unshift((c) => (c.method === "POST" ? json(200, { ok: 1 }) : undefined));
  assert.equal((await r.store.action("menu.key", "05")).status, 200);
  assert.equal((await r.store.action("system.restart")).status, 200);
  const posts = r.f.calls.filter((c) => c.method === "POST");
  assert.deepEqual(posts.map((c) => [c.url, c.body]), [["/api/actions/menu.key", "data=05"], ["/api/actions/system.restart", ""]]);
  assert.equal(r.store.wifi.value!.hostname, "gbscontrol.local");
});

// Found by every-row: status.rev comes once a second, so our write and a TV change can share one revision step; the
// reload after it shows the TV's value and must win over our pending one
test("a newer change from the TV wins over a pending bridged write", async () => {
  const r = await started();
  r.f.auto.unshift((c) => (c.method === "POST" ? json(200, { ok: 1 }) : undefined));
  r.ws.msg("s:status.rev=20");                                          // the revision before our write
  await settle();
  await r.store.writeSettings([["picture.line_filter", 1]]);          // bridged toggle: no echo
  r.set({ ...values, "picture.line_filter": 0 });                      // applied, then turned off on the TV
  r.ws.msg("s:status.rev=22");                                          // one tick for both changes
  await settle();
  assert.equal(r.store.values.value["picture.line_filter"], 0);
  assert.equal(r.store.pendingWrites.value["picture.line_filter"], undefined);
});

test("a reload older than our write keeps the pending value", async () => {
  const r = await started();
  r.f.auto.unshift((c) => (c.method === "POST" ? json(200, { ok: 1 }) : undefined));
  r.ws.msg("s:status.rev=30");
  await settle();
  await r.store.writeSettings([["picture.line_filter", 1]]);
  r.set({ ...values, "picture.line_filter": 0 });                      // the device has not applied it yet
  await r.store.reloadSettings();                                       // a reload with no newer revision
  assert.equal(r.store.values.value["picture.line_filter"], 1);
});

// The echo of an earlier write must not undo a newer one still on its way (hold-to-repeat steppers)
test("a late echo of an earlier write keeps the newer pending value", async () => {
  const r = await started();
  r.f.auto.unshift((c) => (c.method === "POST" ? json(200, { ok: 1 }) : undefined));
  await r.store.writeSettings([["adv.brightness", 11]]);
  await r.store.writeSettings([["adv.brightness", 12]]);
  r.ws.msg("s:adv.brightness=11");                                    // the device answers the first write
  assert.equal(r.store.values.value["adv.brightness"], 12);
  assert.equal(r.store.pendingWrites.value["adv.brightness"]?.value, 12);
  r.ws.msg("s:adv.brightness=12");                                    // then the second
  assert.equal(r.store.values.value["adv.brightness"], 12);
  assert.equal(r.store.pendingWrites.value["adv.brightness"], undefined);
  await r.store.writeSettings([["adv.brightness", 20]]);
  r.ws.msg("s:adv.brightness=33");                                    // a value we never sent: the TV changed it
  assert.equal(r.store.values.value["adv.brightness"], 33);
  assert.equal(r.store.pendingWrites.value["adv.brightness"], undefined);
});

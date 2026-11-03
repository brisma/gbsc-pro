// Device request discipline of the web app (ported from public/src/index.ts)
import { test } from "node:test";
import assert from "node:assert/strict";
import { createClient } from "../../app/api/client.ts";
import { FakeFetch, json, busy, clientDeps, settle } from "./app-helpers.ts";

test("two requests in flight, the third waits", async () => {
  const f = new FakeFetch();
  const c = createClient(clientDeps(f));
  const all = [c.get("/api/a"), c.get("/api/b"), c.get("/api/c")];
  await settle();
  assert.deepEqual(f.pending().map((x) => x.url), ["/api/a", "/api/b"]);
  assert.equal(c.stats().queued, 1);
  f.answer("/api/a", json(200, {}));
  await settle();
  assert.deepEqual(f.pending().map((x) => x.url), ["/api/b", "/api/c"]);
  f.answer("/api/b", json(200, {})); f.answer("/api/c", json(200, {}));
  await Promise.all(all);
});

test("busy 503 retried with growing delay then succeeds", async () => {
  const f = new FakeFetch();
  let n = 0;
  f.auto.push(() => (++n <= 3 ? busy() : json(200, { ok: 1 })));
  const sleeps: number[] = [];
  const r = await createClient(clientDeps(f, sleeps)).get("/api/settings");
  assert.equal(r.status, 200);
  assert.deepEqual(sleeps, [400 + 0.5 * 800 * 1, 400 + 0.5 * 800 * 2, 400 + 0.5 * 800 * 3]);
});

test("busy 503 gives up after 5", async () => {
  const f = new FakeFetch();
  f.auto.push(() => busy());
  const sleeps: number[] = [];
  const r = await createClient(clientDeps(f, sleeps)).get("/api/settings");
  assert.equal(r.status, 503);
  assert.equal(f.calls.length, 6);   // the first try and 5 retries
  assert.equal(sleeps.length, 5);
});

test("409 busy retried twice then reported", async () => {
  const f = new FakeFetch();
  f.auto.push(() => json(409, { error: "busy" }));
  const sleeps: number[] = [];
  const c = createClient(clientDeps(f, sleeps));
  const r = await c.post("/api/settings", { "adv.brightness": "40" });
  assert.equal(r.status, 409);
  assert.equal(f.count("/api/settings", "POST"), 3);
  assert.deepEqual(sleeps, [300, 300]);
  const a = await c.post("/api/actions/picture.reset");
  assert.equal(a.status, 409);
  assert.equal(f.count("/api/actions/picture.reset"), 3);
  f.auto.unshift((x) => (x.url === "/api/profiles/load" ? json(409, { error: "busy" }) : undefined));
  await c.post("/api/profiles/load", { id: "p01" });   // profile ops: no retry (the queue answers busy on purpose)
  assert.equal(f.count("/api/profiles/load"), 1);
});

test("posts carry X-GBSC and a form body", async () => {
  const f = new FakeFetch();
  f.auto.push(() => json(200, { ok: 1 }));
  await createClient(clientDeps(f)).post("/api/settings", { "adv.brightness": "40", "picture.scanlines": "1" });
  assert.equal(f.calls[0].headers["x-gbsc"], "1");
  assert.equal(f.calls[0].body, "adv.brightness=40&picture.scanlines=1");
});

test("upload runs alone and holds the queue", async () => {
  const f = new FakeFetch();
  const c = createClient(clientDeps(f));
  const a = c.get("/api/a");
  await settle();
  const up = c.exclusive("/api/system/update", "BODY");
  const b = c.get("/api/b");
  await settle();
  assert.deepEqual(f.pending().map((x) => x.url), ["/api/a"]);   // the upload waits for the running request
  f.answer("/api/a", json(200, {}));
  await settle(30);
  assert.deepEqual(f.pending().map((x) => x.url), ["/api/system/update"]);   // and /api/b waits for the upload
  assert.equal(f.calls.find((x) => x.url === "/api/system/update")!.headers["x-gbsc"], "1");
  assert.equal((await c.exclusive("/api/system/update", "x")).status, 409);   // a second upload while one runs
  f.answer("/api/system/update", json(202, { ok: 1 }));
  await settle();
  assert.deepEqual(f.pending().map((x) => x.url), ["/api/b"]);
  f.answer("/api/b", json(200, {}));
  await Promise.all([a, up, b]);
});

test("upload retries only a load busy answer", async () => {
  const f = new FakeFetch();
  let n = 0;
  f.auto.push(() => (++n === 1 ? busy("load") : busy("memory")));
  const r = await createClient(clientDeps(f)).exclusive("/api/system/update", "x");
  assert.equal(r.status, 503);
  assert.equal(r.headers.get("X-GBSC-Busy"), "memory");
  assert.equal(f.calls.length, 2);
});

test("get retried after a network error, post not retried", async () => {
  const f = new FakeFetch();
  let n = 0;
  f.auto.push((x) => (x.method === "GET" ? (++n <= 2 ? new TypeError("reset") : json(200, { ok: 1 })) : new TypeError("reset")));
  const c = createClient(clientDeps(f));
  assert.equal((await c.get("/api/settings")).status, 200);
  assert.equal(f.count("/api/settings", "GET"), 3);
  await assert.rejects(c.post("/api/settings", { a: "1" }));
  assert.equal(f.count("/api/settings", "POST"), 1);
});

test("info shared within 1500 ms", async () => {
  const f = new FakeFetch();
  f.auto.push(() => json(200, { fw: "3.0.0" }));
  let now = 1000;
  const c = createClient({ ...clientDeps(f), now: () => now });
  const [a, b] = await Promise.all([c.info(), c.info()]);
  assert.equal(a.fw, "3.0.0"); assert.equal(b.fw, "3.0.0");
  now += 1400; await c.info();
  assert.equal(f.count("/api/info"), 1);
  now += 200; await c.info();
  assert.equal(f.count("/api/info"), 2);
});

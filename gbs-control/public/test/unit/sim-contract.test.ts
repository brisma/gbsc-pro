// The simulator against data generated from the firmware: the validators (sim/vectors -> gen/validate.json), the
// routes both ways (gen/routes.json), the /api/info keys and the log line format (gen/logsample.json)
import { test, before, after } from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { join } from "node:path";
import { startSim, routeHandled, type Sim } from "../../sim/server.ts";
import { req, fixturePage, publicDir } from "./sim-helpers.ts";
import { validateRequest } from "../../sim/validate.ts";
import { validateAction } from "../../sim/actions-validate.ts";
import { BOARD_CAPS, type Board } from "../../sim/types.ts";

const gen = (f: string) => JSON.parse(readFileSync(join(publicDir, "gen", f), "utf8"));
const vectors = (f: string) => JSON.parse(readFileSync(join(publicDir, "sim", "vectors", f), "utf8"));

let sim: Sim;
before(async () => { sim = await startSim({ port: 0, wsPort: 0, pagePath: fixturePage }); });
after(async () => { await sim.stop(); });

test("sim validator matches the firmware vectors", () => {
  const registry = gen("registry.json");
  const expected = gen("validate.json").settings;
  const input = vectors("settings-requests.json");
  assert.equal(expected.length, input.length);
  input.forEach((v: any, i: number) => {
    const r = validateRequest(registry, BOARD_CAPS[v.board as Board], v.devmode, v.keys, v.values);
    const want = expected[i];
    assert.equal(want.name, v.name);
    assert.deepEqual({ ok: r.ok, errors: r.errors, pairs: r.pairs, str: r.str },
                     { ok: want.ok, errors: want.errors, pairs: want.pairs, str: want.str }, v.name);
  });
});

test("sim action validation matches the firmware vectors", () => {
  const all = gen("actions-all.json");
  const expected = gen("validate.json").actions;
  const input = vectors("action-requests.json");
  input.forEach((v: any, i: number) => {
    const r = validateAction(all, BOARD_CAPS[v.board as Board], v.devmode, v.action, v.data);
    assert.equal(r.status, expected[i].status, v.name);
    assert.deepEqual(r.parsed, expected[i].parsed, v.name);
  });
});

const routes = gen("routes.json");
const sample = (r: { path: string; match: string }) => (r.match === "prefix" ? r.path + "system.restart" : r.path);

test("every firmware route has a handler", () => {
  for (const r of routes.normal) for (const m of r.methods) assert.ok(routeHandled(sim, m, sample(r), false), `${m} ${r.path}`);
  for (const r of routes.safe) for (const m of r.methods) assert.ok(routeHandled(sim, m, sample(r), true), `safe ${m} ${r.path}`);
});

test("no handler without a firmware route", () => {
  for (const h of sim.routes) {
    if (h.path.startsWith("/__sim/")) continue;
    const inTable = (rows: any[]) => rows.some((r) => r.path === h.path && r.methods.includes(h.method) && (r.match === "prefix") === h.prefix);
    if (h.normal) assert.ok(inTable(routes.normal), `normal ${h.method} ${h.path}`);
    if (h.safe) assert.ok(inTable(routes.safe), `safe ${h.method} ${h.path}`);
  }
});

test("info has every firmware key", async () => {
  const r = await req(sim.url, "GET", "/api/info");
  for (const k of ["api", "fw", "marker", "build", "board", "caps", "probes", "heap", "max_block", "wifi", "adv_controller",
                   "net", "clockgen", "input", "signal", "crash", "upload", "update"]) assert.ok(k in r.json, k);
  for (const k of ["none", "std", "lines", "src_hz10", "out_hz10", "interlaced", "scanlines_ok", "adv_ntsc", "preset"])
    assert.ok(k in r.json.signal, k);
  assert.equal(r.json.fw, "3.0.0-dev0");
});

test("log lines match the firmware format", async () => {
  const samples = gen("logsample.json");
  const { formatLogLine } = await import("../../sim/state.ts");
  for (const s of samples) assert.equal(formatLogLine(s.record), s.line);
  const r = await req(sim.url, "GET", "/api/log?since=0");
  assert.ok(Array.isArray(r.json.lines) && r.json.lines.length > 0);
  assert.equal(typeof r.json.boot, "number");
  const next = r.json.next;
  sim.state.log("api", "info", "one more line");
  const r2 = await req(sim.url, "GET", `/api/log?since=${next}&boot=${r.json.boot}`);
  assert.equal(r2.json.lines.length, 1);
  assert.equal(r2.json.lines[0].m, "one more line");
});

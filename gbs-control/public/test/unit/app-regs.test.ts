// Register model of the Developer tools: the map file, the ADV write planner, the ADV Manager YAML subset and the
// TV5725 field writes
import { test } from "node:test";
import assert from "node:assert/strict";
import { readFileSync, readdirSync } from "node:fs";
import { join } from "node:path";
import { parseRegmap, fieldValue, withField, type TvField } from "../../app/regs/regmap.ts";
import { planAdvWrites, planAdvReads, runReads, type AdvChange } from "../../app/regs/planner.ts";
import { parseAdvYaml, writeAdvYaml } from "../../app/regs/yaml.ts";
import { planTvWrite, planTvWrites } from "../../app/regs/tv5725.ts";
import { publicDir } from "./sim-helpers.ts";

const map = parseRegmap(JSON.parse(readFileSync(join(publicDir, "gen", "regmap.json"), "utf8")));
const ADV = join(publicDir, "..", "..", "shared", "adv-regmap");
const w = (i2c: number, mp: number, reg: number, value: number): AdvChange => ({ i2c, map: mp, reg, value });

test("field value extraction", () => {
  const tv = map.tv5725.find((f) => f.name === "VDS_HSCALE")!;   // 3_16, 10 bits from bit 0
  assert.deepEqual([tv.seg, tv.reg, tv.bit, tv.width], [3, 0x16, 0, 10]);
  const seg = new Uint8Array(256);
  seg[0x16] = 0x49; seg[0x17] = 0xF2;   // 0x249: the upper bits of 0x17 belong to VDS_VSCALE
  assert.equal(fieldValue(seg, tv), 0x249);
  const one: TvField = { name: "X", seg: 0, reg: 0x40, bit: 5, width: 1, access: "rw" };
  seg[0x40] = 0x20;
  assert.equal(fieldValue(seg, one), 1);
  const reg = map.chips.find((c) => c.id === "ADV7280")!.maps[0].regs.find((r) => r.addr === 0x00)!;
  assert.equal(reg.def, 0x09);
  assert.equal(map.danger.get("42:00:0e"), "planner");
});

test("wide field write keeps the other bits", () => {
  const seg = new Uint8Array(256).fill(0xFF);
  const f11: TvField = { name: "W11", seg: 1, reg: 0x1A, bit: 3, width: 11, access: "rw" };   // 0x1A bits 3-7, 0x1B bits 0-5
  const out = withField(seg, f11, 0);
  assert.equal(out[0x1A], 0x07);
  assert.equal(out[0x1B], 0xC0);
  assert.equal(fieldValue(withField(seg, f11, 0x5A5), f11), 0x5A5);
  assert.equal(planTvWrite(f11, 0, seg), "01,1A,07,01,1B,C0");
  const f32: TvField = { name: "W32", seg: 5, reg: 0x10, bit: 0, width: 32, access: "rw" };
  const o32 = withField(new Uint8Array(256), f32, 0xDEADBEEF);
  assert.deepEqual([...o32.slice(0x10, 0x14)], [0xEF, 0xBE, 0xAD, 0xDE]);
  assert.equal(fieldValue(o32, f32), 0xDEADBEEF);
  const f32b: TvField = { name: "W32B", seg: 5, reg: 0x20, bit: 4, width: 32, access: "rw" };   // 5 bytes
  const s5 = new Uint8Array(256).fill(0xFF);
  const o5 = withField(s5, f32b, 0);
  assert.deepEqual([...o5.slice(0x20, 0x25)], [0x0F, 0, 0, 0, 0xF0]);
  assert.equal(planTvWrite(f32b, 0, s5).split(",").length, 15);
});

test("planner never splits a map group", () => {
  const ch = Array.from({ length: 12 }, (_, i) => w(0x42, 0x40, 0x80 + i, i));
  const p = planAdvWrites(ch, map);
  for (const r of p.requests) {
    const b = r.split(",");
    assert.ok(b.length / 3 <= 10, r);
    assert.deepEqual(b.slice(0, 3), ["42", "0E", "40"]);   // every request opens and closes its own group
    assert.deepEqual(b.slice(-3), ["42", "0E", "00"]);
  }
  assert.equal(p.requests.map((r) => r.split(",").length / 3 - 2).reduce((a, b) => a + b, 0), 12);
});

test("planner refuses maps on the encoder and VPP", () => {
  const p = planAdvWrites([w(0x56, 0x40, 0x01, 1), w(0x84, 0x20, 0x41, 0), w(0x42, 0x60, 0x01, 0), w(0x56, 0, 0x01, 0x70)], map);
  assert.deepEqual(p.skipped.map((s) => s.reason), ["no map on this chip", "no map on this chip", "unknown map"]);
  assert.deepEqual(p.requests, ["56,01,70"]);
});

test("planner merges repeated registers", () => {
  const p = planAdvWrites([w(0x42, 0, 0x0A, 1), w(0x56, 0, 0x01, 2), w(0x42, 0, 0x0A, 3)], map);
  assert.deepEqual(p.requests, ["42,0A,03,56,01,02"]);
});

test("planner splits main-map lists at 10 triplets", () => {
  const writable = map.chips[0].maps[0].regs.filter((r) => !r.readonly && !r.danger).slice(0, 23);
  assert.equal(writable.length, 23);
  const p = planAdvWrites(writable.map((r) => w(0x42, 0, r.addr, 0)), map);
  assert.deepEqual(p.requests.map((r) => r.split(",").length / 3), [10, 10, 3]);
});

test("planner skips 0x0E in every ADV7280 map", () => {
  const p = planAdvWrites([w(0x42, 0x20, 0x0E, 0x40), w(0x42, 0x40, 0x0E, 0x00), w(0x42, 0x40, 0x80, 0x80)], map);
  assert.deepEqual(p.skipped.map((s) => [s.reg.map, s.reason]), [[0x20, "map select"], [0x40, "map select"]]);
  assert.deepEqual(p.requests, ["42,0E,40,42,80,80,42,0E,00"]);
});

test("planner skips 0x0E and read-only registers and lists dangerous ones", () => {
  const ro = map.chips[0].maps[0].regs.find((r) => r.readonly)!;
  const p = planAdvWrites([w(0x42, 0, 0x0E, 0x40), w(0x42, 0, ro.addr, 1), w(0x42, 0, 0x0F, 0x80), w(0x56, 0, 0x17, 0x02), w(0x42, 0, 0x0A, 0x10)], map);
  assert.deepEqual(p.skipped.map((s) => [s.reg.reg, s.reason]), [[0x0E, "map select"], [ro.addr, "read-only"]]);
  assert.deepEqual(p.dangerous.map((d) => [d.i2c, d.reg]), [[0x42, 0x0F], [0x56, 0x17]]);
  assert.deepEqual(p.requests, ["42,0F,80,56,17,02,42,0A,10"]);
});

test("reads grouped by 32 and never cross 0xFF", () => {
  const regs = [0x00, 0x01, 0x1F, 0x20, 0x40, 0xF0, 0xFF].map((r) => ({ i2c: 0x42, map: 0, reg: r }));
  const r = planAdvReads([...regs, { i2c: 0x42, map: 0x40, reg: 0x80 }, { i2c: 0x84, map: 0, reg: 0x41 }]);
  assert.deepEqual(r, [
    { dev: 0x42, map: 0, reg: 0x00, count: 32 }, { dev: 0x42, map: 0, reg: 0x20, count: 1 }, { dev: 0x42, map: 0, reg: 0x40, count: 1 },
    { dev: 0x42, map: 0, reg: 0xF0, count: 16 }, { dev: 0x42, map: 0x40, reg: 0x80, count: 1 }, { dev: 0x84, map: 0, reg: 0x41, count: 1 },
  ]);
  for (const q of r) assert.ok(q.count <= 32 && q.reg + q.count <= 0x100);
});

test("reads are serialised", async () => {
  let inFlight = 0, most = 0;
  const seen: number[] = [];
  const out = await runReads([{ dev: 0x42, map: 0, reg: 0, count: 1 }, { dev: 0x42, map: 0, reg: 1, count: 1 }, { dev: 0x56, map: 0, reg: 0, count: 1 }],
    async (q) => { inFlight++; most = Math.max(most, inFlight); await new Promise((r) => setTimeout(r, 5)); inFlight--; return q.reg; },
    (q, v) => seen.push(v));
  assert.equal(most, 1);
  assert.deepEqual(seen, [0, 1, 0]);
  assert.deepEqual(out, [0, 1, 0]);
});

test("yaml round trip is stable for every adv-regmap file", () => {
  for (const dir of ["presets", "macros"]) {
    for (const f of readdirSync(join(ADV, dir)).filter((n) => /\.ya?ml$/.test(n))) {
      const text = readFileSync(join(ADV, dir, f), "utf8");
      const a = parseAdvYaml(text);
      assert.ok(a.ok, `${dir}/${f}: ${a.ok ? "" : a.error}`);
      const once = writeAdvYaml(a.ok ? a.doc : (null as never));
      const b = parseAdvYaml(once);
      assert.ok(b.ok);
      assert.equal(writeAdvYaml(b.ok ? b.doc : (null as never)), once, `${dir}/${f}`);
    }
  }
  const d = parseAdvYaml(readFileSync(join(ADV, "presets", "default.yaml"), "utf8"));
  assert.ok(d.ok && d.doc.kind === "preset");
  if (d.ok && d.doc.kind === "preset") {
    assert.equal(d.doc.name, "Default");
    const main = d.doc.devices.find((x) => x.chip === "ADV7280")!.maps.find((m) => m.id === "user_sub_map")!;
    assert.deepEqual(main.regs[0], [0x00, 0x09]);
  }
});

test("macro unknown lines kept and marked", () => {
  const r = parseAdvYaml("name: Mixed\ncommands:\n  - wm 42 40 80 80\n  - w 56 17 02\n  - delay 100\n");
  assert.ok(r.ok && r.doc.kind === "macro");
  if (!r.ok || r.doc.kind !== "macro") return;
  assert.deepEqual(r.doc.commands.map((c) => [c.text, c.run]), [["wm 42 40 80 80", true], ["w 56 17 02", true], ["delay 100", false]]);
  assert.deepEqual(r.doc.commands[0].write, w(0x42, 0x40, 0x80, 0x80));
  assert.match(writeAdvYaml(r.doc), /  - delay 100\n/);
});

test("legacy preset refused with the message", () => {
  const legacy = "name: Old\ndevices:\n  ADV7280:\n    maps:\n      default:\n        registers:\n          '0x00': '0x09'\n";
  assert.deepEqual(parseAdvYaml(legacy), { ok: false, error: "old ADV Manager preset layout (no register maps): not supported" });
  assert.equal(parseAdvYaml("name: Flat\nregisters:\n  '0x00': '0x09'\n").ok, false);
  assert.equal(parseAdvYaml("description: nameless\ncommands:\n  - w 42 0A 10\n").ok, true);   // the page names it from the file
});

test("several fields of one register merge into one write, 10 triplets per request", () => {
  const seg = new Uint8Array(256);
  const a: TvField = { name: "A", seg: 0, reg: 0x46, bit: 0, width: 1, access: "confirm" };
  const b: TvField = { name: "B", seg: 0, reg: 0x46, bit: 4, width: 1, access: "confirm" };
  assert.deepEqual(planTvWrites([[a, 1], [b, 1]], seg), ["00,46,11"]);
  const many: [TvField, number][] = Array.from({ length: 12 }, (_, i) => [{ name: "F" + i, seg: 3, reg: i, bit: 0, width: 8, access: "rw" }, i]);
  assert.deepEqual(planTvWrites(many, new Uint8Array(256)).map((r) => r.split(",").length / 3), [10, 2]);
});

test("yaml quotes names and lines PyYAML would read as another type", () => {
  for (const n of ["On", "off", "Yes", "NO", "true", "null", "~", "123", "1.5", "0x10"]) {
    const t = writeAdvYaml({ kind: "macro", name: n, commands: [] });
    assert.match(t, new RegExp(`^name: '${n.replace(/[.]/g, "\\.")}'\\n`), n);
    const r = parseAdvYaml(t);
    assert.ok(r.ok && r.doc.name === n, n);
  }
  assert.match(writeAdvYaml({ kind: "macro", name: "Soft composite", commands: [] }), /^name: Soft composite\n/);
  const t = writeAdvYaml({ kind: "macro", name: "M", commands: [{ text: "delay: 100", run: false }, { text: "w 42 0A 10", run: true }] });
  assert.match(t, /  - 'delay: 100'\n  - w 42 0A 10\n/);
  const r = parseAdvYaml(t);
  assert.ok(r.ok && r.doc.kind === "macro");
  if (r.ok && r.doc.kind === "macro") assert.deepEqual(r.doc.commands.map((c) => c.text), ["delay: 100", "w 42 0A 10"]);
});

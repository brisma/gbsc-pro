// ADV presets and macros model: presets from the values applied now, their writes, macro steps
import { test } from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { join } from "node:path";
import { parseRegmap, advKey } from "../../app/regs/regmap.ts";
import { parseAdvYaml, writeAdvYaml } from "../../app/regs/yaml.ts";
import { presetFromApplied, presetChanges, macroChanges, stepsToCommands, withName, defaultPreset } from "../../app/regs/adv-model.ts";
import { publicDir } from "./sim-helpers.ts";

const map = parseRegmap(JSON.parse(readFileSync(join(publicDir, "gen", "regmap.json"), "utf8")));

function appliedNow(): Map<string, number> {
  const v = new Map<string, number>();
  for (const c of map.chips) for (const m of c.maps) for (const r of m.regs) v.set(advKey(m.i2c, m.map, r.addr), r.def);
  v.set(advKey(0x42, 0, 0x0a), 0x10);    // brightness moved
  v.set(advKey(0x42, 0x40, 0x80), 0x80); // ACE on
  return v;
}

test("applied-now preset contains only registers different from Default", () => {
  const p = presetFromApplied(map, appliedNow(), "changed", "Bright", "brighter");
  assert.equal(p.kind, "preset");
  assert.deepEqual(p.devices.map((d) => [d.chip, d.maps.map((m) => [m.id, m.mapValue, m.regs])]),
    [["ADV7280", [["user_sub_map", 0, [[0x0a, 0x10]]], ["user_sub_map_2", 0x40, [[0x80, 0x80]]]]]]);
  const all = presetFromApplied(map, appliedNow(), "all", "All", "");
  assert.ok(all.devices.length === 2 && all.devices.every((d) => d.maps.every((m) => m.regs.length)));
  const enc = presetFromApplied(map, appliedNow(), "enc", "Enc", "");
  assert.deepEqual(enc.devices.map((d) => d.chip), ["ADV7391"]);
  for (const d of all.devices) for (const m of d.maps) for (const [reg] of m.regs)   // never 0x0E or read-only
    assert.ok(!(d.chip === "ADV7280" && m.id === "user_sub_map" && reg === 0x0e));
});

test("preset writes resolve chip and map names", () => {
  const p = presetFromApplied(map, appliedNow(), "changed", "Bright", "");
  assert.deepEqual(presetChanges(p, map), [{ i2c: 0x42, map: 0, reg: 0x0a, value: 0x10 }, { i2c: 0x42, map: 0x40, reg: 0x80, value: 0x80 }]);
  const d = parseAdvYaml(readFileSync(join(publicDir, "..", "..", "shared", "adv-regmap", "presets", "default.yaml"), "utf8"));
  assert.ok(d.ok && d.doc.kind === "preset");
  const ch = d.ok && d.doc.kind === "preset" ? presetChanges(d.doc, map) : [];
  assert.ok(ch.some((c) => c.i2c === 0x84 && c.map === 0 && c.reg === 0x41));   // the VPP map by its own address
  assert.equal(defaultPreset(map).name, "Default");
});

test("macro steps become w and wm lines from the known register values", () => {
  const v = appliedNow();
  const lines = stepsToCommands([
    { kind: "field", i2c: 0x42, map: 0x40, reg: 0x80, lsb: 7, x: 0 },
    { kind: "reg", i2c: 0x56, map: 0, reg: 0x17, value: 0x02 },
    { kind: "field", i2c: 0x42, map: 0, reg: 0x0a, lsb: 0, x: 0x20 },
  ], map, v);
  assert.deepEqual(lines, ["wm 42 40 80 00", "w 56 17 02", "w 42 0A 20"]);
  const r = parseAdvYaml(writeAdvYaml({ kind: "macro", name: "M", commands: lines.map((t) => ({ text: t, run: true })) }));
  assert.ok(r.ok && r.doc.kind === "macro");
  if (r.ok && r.doc.kind === "macro") assert.deepEqual(macroChanges(r.doc), [{ i2c: 0x42, map: 0x40, reg: 0x80, value: 0 }, { i2c: 0x56, map: 0, reg: 0x17, value: 2 }, { i2c: 0x42, map: 0, reg: 0x0a, value: 0x20 }]);
});

test("a file without a name takes the file name", () => {
  assert.equal(withName("description: x\ncommands:\n  - w 42 0A 10\n", "soft_rf.yaml"), "name: soft_rf\ndescription: x\ncommands:\n  - w 42 0A 10\n");
  assert.equal(withName("name: Kept\n", "other.yml"), "name: Kept\n");
});

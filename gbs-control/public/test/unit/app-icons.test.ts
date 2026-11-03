// Icon subset: every icon the app names is in the embedded Material Symbols subset
import { test } from "node:test";
import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import { mkdtempSync, writeFileSync, readFileSync, mkdirSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { publicDir } from "./sim-helpers.ts";

const run = (...args: string[]) => spawnSync(process.execPath, [join(publicDir, "tools", "icons-app.mjs"), ...args], { encoding: "utf8" });

test("every icon used is in the subset", () => {
  const dir = mkdtempSync(join(tmpdir(), "icons-"));
  mkdirSync(join(dir, "app"));
  writeFileSync(join(dir, "app", "x.tsx"), 'const a = { icon: "tune" }; <Icon name="wifi" />; <Row icon="zzz_none" />;\n');
  const bad = run("--src", join(dir, "app"), "--out", join(dir, "icons.json"));
  assert.equal(bad.status, 1, bad.stdout + bad.stderr);
  assert.match(bad.stdout + bad.stderr, /zzz_none/);
  writeFileSync(join(dir, "app", "x.tsx"), 'const a = { icon: "tune" }; <Icon name="wifi" />;\n');
  const ok = run("--src", join(dir, "app"), "--out", join(dir, "icons.json"));
  assert.equal(ok.status, 0, ok.stdout + ok.stderr);
  const j = JSON.parse(readFileSync(join(dir, "icons.json"), "utf8"));
  assert.deepEqual(Object.keys(j.map).sort(), ["tune", "wifi"]);
  assert.match(j.font, /^data:font\/woff2;base64,/);
  assert.ok(j.font.length < 6000, `two icons stay small: ${j.font.length}`);
});

test("names inside expressions and marked lines are collected", () => {
  const dir = mkdtempSync(join(tmpdir(), "icons-"));
  mkdirSync(join(dir, "app"));
  writeFileSync(join(dir, "app", "x.tsx"), [
    '<Icon name={open ? "expand_less" : "expand_more"} />',
    'const G = { "A b": "memory" };   // @icons',
    '<Row icon={on ? "check" : "close"} />',
    'tool({ icon: paused ? "play_arrow" : "pause", label: "x" });',
  ].join("\n"));
  const r = run("--src", join(dir, "app"), "--out", join(dir, "icons.json"));
  assert.equal(r.status, 0, r.stdout + r.stderr);
  const j = JSON.parse(readFileSync(join(dir, "icons.json"), "utf8"));
  assert.deepEqual(Object.keys(j.map).sort(), ["check", "close", "expand_less", "expand_more", "memory", "pause", "play_arrow"]);
});

test("the app tree builds its subset", () => {
  const dir = mkdtempSync(join(tmpdir(), "icons-"));
  const r = run("--out", join(dir, "icons.json"));
  assert.equal(r.status, 0, r.stdout + r.stderr);
});

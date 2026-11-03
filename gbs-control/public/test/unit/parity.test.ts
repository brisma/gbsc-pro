// Parity (no hand-kept list): every setting, menu node, action and route of the firmware tables
// is used somewhere in the web app, or is an exclusion with a reason (test/parity-exclusions.json)
import { test } from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, cpSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { spawnSync } from "node:child_process";
import { publicDir } from "./sim-helpers.ts";

const run = (args: string[]) => spawnSync(process.execPath, [join(publicDir, "tools", "parity.mjs"), ...args], { encoding: "utf8", cwd: publicDir });
const genCopy = () => { const g = mkdtempSync(join(tmpdir(), "parity-gen-")); cpSync(join(publicDir, "gen"), g, { recursive: true }); return g; };

test("parity passes on the web app as it is", () => {
  const r = run([]);
  assert.equal(r.status, 0, r.stdout + r.stderr);
  assert.match(r.stdout, /covered \d+, excluded \d+/);
});

test("parity names a new setting no page uses", () => {
  const gen = genCopy();
  const reg = JSON.parse(readFileSync(join(gen, "registry.json"), "utf8"));
  reg.settings.push({ ...reg.settings[0], key: "zz.brand_new" });
  writeFileSync(join(gen, "registry.json"), JSON.stringify(reg));
  const r = run(["--gen", gen]);
  assert.equal(r.status, 1);
  assert.match(r.stdout, /setting:zz\.brand_new/);
});

test("parity names a menu node the web app never shows", () => {
  const gen = genCopy();
  const menu = JSON.parse(readFileSync(join(gen, "menu.json"), "utf8"));
  const input = menu.root.children.find((c: { id: string }) => c.id === "input");
  input.children.push({ id: "input/zz-hidden", type: "action", label: "Hidden", help: "x", flags: [], pred: "none", action: "zz.hidden" });
  writeFileSync(join(gen, "menu.json"), JSON.stringify(menu));
  const r = run(["--gen", gen]);
  assert.equal(r.status, 1);
  assert.match(r.stdout, /menu:input\/zz-hidden/);
});

test("parity names a stale exclusion", () => {
  const ex = JSON.parse(readFileSync(join(publicDir, "test", "parity-exclusions.json"), "utf8"));
  ex.push({ ref: "setting:old.gone", reason: "gone" });
  const file = join(mkdtempSync(join(tmpdir(), "parity-x-")), "ex.json");
  writeFileSync(file, JSON.stringify(ex));
  const r = run(["--exclusions", file]);
  assert.equal(r.status, 1);
  assert.match(r.stdout, /stale exclusion: setting:old\.gone/);
});

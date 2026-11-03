// Size gate and inlined build
import { test } from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, writeFileSync, readFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { spawnSync } from "node:child_process";
import { randomBytes } from "node:crypto";
import { publicDir } from "./sim-helpers.ts";

const tool = (name: string, args: string[]) =>
  spawnSync(process.execPath, [join(publicDir, "tools", name), ...args], { encoding: "utf8", cwd: publicDir });

test("size gate passes under the limit", () => {
  const f = join(mkdtempSync(join(tmpdir(), "size-")), "a.html");
  writeFileSync(f, randomBytes(1000));
  const r = tool("size.mjs", [f]);
  assert.equal(r.status, 0, r.stdout + r.stderr);
  assert.match(r.stdout, /a\.html\.gz: \d+ B of 140000 B/);
});

test("size gate fails over the limit with the numbers", () => {
  const f = join(mkdtempSync(join(tmpdir(), "size-")), "a.html");
  writeFileSync(f, randomBytes(1000));
  const r = tool("size.mjs", ["--limit", "10", f]);
  assert.equal(r.status, 1);
  assert.match(r.stdout, /of 10 B/);
});

test("build output loads nothing from outside", () => {
  const b = tool("build-app.mjs", []);
  assert.equal(b.status, 0, b.stdout + b.stderr);
  const html = readFileSync(join(publicDir, "dist", "app.html"), "utf8");
  // namespaces, and the project page the footer links to (a link the user follows: the page loads nothing from it)
  const allowed = ["http://www.w3.org/2000/svg", "http://www.w3.org/1999/xlink", "http://www.w3.org/1998/Math/MathML", "http://www.w3.org/1999/xhtml",
    "https://github.com/brisma/gbsc-pro"];
  const urls = [...html.matchAll(/https?:\/\/[^\s"'`)<>]+/g)].map((m) => m[0]).filter((u) => !allowed.some((a) => u.startsWith(a)));
  assert.deepEqual(urls, []);
  assert.match(html, /<script>/);
  assert.ok(!/<script[^>]+src=/.test(html), "no external script");
  assert.ok(![...html.matchAll(/<link[^>]+href="([^"]*)"/g)].some((m) => !m[1].startsWith("data:")), "no external link");
});

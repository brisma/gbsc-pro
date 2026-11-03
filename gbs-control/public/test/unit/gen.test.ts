// gen/ freshness: gen-check names the firmware sources that changed since npm run gen
import { test } from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, mkdirSync, writeFileSync, appendFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, dirname } from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";

const tools = join(dirname(fileURLToPath(import.meta.url)), "..", "..", "tools");

function fakeTree(): string {
  const root = mkdtempSync(join(tmpdir(), "gencheck-"));
  const files: Record<string, string> = {
    "platformio.ini": "[env:native]\nbuild_src_filter = -<*> +<pro/ui/menu-tree.cpp> +<pro/settings/settings-registry.cpp>\n",
    "pro/ui/menu-tree.cpp": "// tree\n",
    "pro/ui/menu-types.h": "// types\n",
    "pro/settings/settings-registry.cpp": "// registry\n",
    "test/sim/menu-fake.cpp": "// fake\n",
    "test/test_web_export/test_main.cpp": "// exporter\n",
    "public/sim/vectors/settings-requests.json": "[]\n",
  };
  for (const [p, t] of Object.entries(files)) {
    mkdirSync(dirname(join(root, p)), { recursive: true });
    writeFileSync(join(root, p), t);
  }
  return root;
}

const run = (script: string, args: string[]) =>
  spawnSync(process.execPath, [join(tools, script), ...args], { encoding: "utf8" });

test("gen freshness detects a changed source", () => {
  const root = fakeTree();
  const w = run("gen.mjs", ["--root", root, "--manifest-only"]);
  assert.equal(w.status, 0, w.stderr);
  const ok = run("gen-check.mjs", ["--root", root]);
  assert.equal(ok.status, 0, ok.stdout + ok.stderr);
  appendFileSync(join(root, "pro/ui/menu-tree.cpp"), "x");
  const stale = run("gen-check.mjs", ["--root", root]);
  assert.equal(stale.status, 1);
  assert.match(stale.stdout, /pro\/ui\/menu-tree\.cpp/);
});

test("gen-check without gen dir asks to run gen", () => {
  const root = fakeTree();
  const r = run("gen-check.mjs", ["--root", root]);
  assert.equal(r.status, 1);
  assert.match(r.stdout, /npm run gen/);
});

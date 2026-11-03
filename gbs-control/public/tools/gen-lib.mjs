// Shared by gen.mjs and gen-check.mjs: the firmware sources gen/ is built from, and their hashes
import { readFileSync, readdirSync, statSync, existsSync } from "node:fs";
import { createHash } from "node:crypto";
import { join, relative } from "node:path";

export const sha256 = (path) => createHash("sha256").update(readFileSync(path)).digest("hex");

function walk(dir, keep, out) {
  if (!existsSync(dir)) return;
  for (const name of readdirSync(dir)) {
    const p = join(dir, name);
    if (statSync(p).isDirectory()) walk(p, keep, out);
    else if (keep(name)) out.push(p);
  }
}

// Every file of the native build_src_filter, every header under pro/, test/sim/, the exporter test, the vectors and
// the register map inputs
export function listSources(root) {
  const out = new Set();
  const ini = readFileSync(join(root, "platformio.ini"), "utf8");
  for (const line of ini.split("\n")) {
    if (!line.startsWith("build_src_filter") || !line.includes("+<pro/")) continue;
    for (const m of line.matchAll(/\+<([^>*]+)>/g)) if (existsSync(join(root, m[1]))) out.add(join(root, m[1]));
  }
  const acc = [];
  walk(join(root, "pro"), (n) => n.endsWith(".h"), acc);
  walk(join(root, "test", "sim"), () => true, acc);
  walk(join(root, "public", "sim", "vectors"), (n) => n.endsWith(".json"), acc);
  const exporter = join(root, "test", "test_web_export", "test_main.cpp");
  if (existsSync(exporter)) acc.push(exporter);
  // gen/regmap.json (tools/regmap.py at the repo root): the ADV register maps, default preset and macros
  // (shared/adv-regmap), tv5725.h and the
  // ADV Controller golden traces (the registers it sets)
  const repo = join(root, "..");
  for (const f of [join(root, "tv5725.h"), join(repo, "tools", "regmap.py"), join(repo, "shared", "adv-regmap", "presets", "default.yaml")])
    if (existsSync(f)) acc.push(f);
  walk(join(repo, "shared", "adv-regmap", "configs"), (n) => n.endsWith(".yaml"), acc);
  walk(join(repo, "shared", "adv-regmap", "macros"), (n) => n.endsWith(".yaml"), acc);
  walk(join(repo, "adv-controller", "test", "golden"), (n) => n.endsWith(".txt"), acc);
  for (const p of acc) out.add(p);
  const rel = {};
  for (const p of [...out].sort()) rel[relative(root, p).split("\\").join("/")] = sha256(p);
  return rel;
}

export function parseRoot(argv, fallback) {
  const i = argv.indexOf("--root");
  return i >= 0 ? argv[i + 1] : fallback;
}

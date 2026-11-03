// npm run gen: runs the native exporter (test_web_export) and tools/regmap.py into public/gen/ and writes gen/manifest.json
// (sha256 of the firmware sources and of every generated file). --manifest-only skips the exporter (tests).
import { writeFileSync, mkdirSync, readdirSync } from "node:fs";
import { join, dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";
import { listSources, sha256, parseRoot } from "./gen-lib.mjs";
import { python } from "./python.mjs";

const root = resolve(parseRoot(process.argv, join(dirname(fileURLToPath(import.meta.url)), "..", "..")));
const gen = join(root, "public", "gen");
mkdirSync(gen, { recursive: true });

if (!process.argv.includes("--manifest-only")) {
  console.log(`gen: running the native exporter into ${gen}`);
  // its own build dir: gen also runs inside a firmware build (the webui-build.py pre-script), and a pio that finds the
  // project's files changed (the pre-scripts add headers) cleans its whole build dir, the outer build's included
  const buildDir = join(process.env.PLATFORMIO_BUILD_DIR || join(root, ".pio", "build"), "web-export");
  const r = spawnSync("pio", ["test", "-e", "native", "-f", "test_web_export"], {
    cwd: root, stdio: "inherit", env: { ...process.env, WEB_EXPORT_DIR: gen, PLATFORMIO_BUILD_DIR: buildDir },
  });
  if (r.error) { console.error(`gen: cannot run pio (${r.error.message}); PlatformIO and a host C++ compiler are needed`); process.exit(2); }
  if (r.status !== 0) { console.error("gen: the exporter failed"); process.exit(r.status ?? 1); }
}

if (!process.argv.includes("--manifest-only")) {
  const r = spawnSync(python(), [join(root, "..", "tools", "regmap.py"), "--repo", join(root, ".."), "--out", join(gen, "regmap.json")], { stdio: "inherit" });
  if (r.error || r.status !== 0) { console.error("gen: tools/regmap.py failed (python3 with PyYAML is needed: pip install pyyaml)"); process.exit(r.status || 2); }
}

const files = {};
for (const name of readdirSync(gen).sort()) if (name !== "manifest.json") files[name] = sha256(join(gen, name));
writeFileSync(join(gen, "manifest.json"), JSON.stringify({ sources: listSources(root), files }, null, 1) + "\n");
console.log(`gen: ${Object.keys(files).length} files, manifest written`);

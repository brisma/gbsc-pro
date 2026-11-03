// npm run gen:check: fails when a firmware source changed since npm run gen (gen/ would be stale)
import { readFileSync, existsSync } from "node:fs";
import { join, dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { listSources, sha256, parseRoot } from "./gen-lib.mjs";

const root = resolve(parseRoot(process.argv, join(dirname(fileURLToPath(import.meta.url)), "..", "..")));
const manifestPath = join(root, "public", "gen", "manifest.json");
if (!existsSync(manifestPath)) { console.log("gen-check: public/gen is missing: run npm run gen"); process.exit(1); }
const manifest = JSON.parse(readFileSync(manifestPath, "utf8"));
const now = listSources(root);
const changed = [];
for (const [p, h] of Object.entries(now)) if (manifest.sources[p] !== h) changed.push(manifest.sources[p] ? p : `${p} (new)`);
for (const p of Object.keys(manifest.sources)) if (!(p in now)) changed.push(`${p} (removed)`);
for (const [name, h] of Object.entries(manifest.files)) {
  const f = join(root, "public", "gen", name);
  if (!existsSync(f) || sha256(f) !== h) changed.push(`public/gen/${name} (edited or missing)`);
}
if (changed.length) {
  console.log(`gen-check: stale, run npm run gen. Changed since the last export:\n  ${changed.join("\n  ")}`);
  process.exit(1);
}
console.log(`gen-check: up to date (${Object.keys(now).length} sources)`);

// Icon subset of the web app: collects icon="…", icon: "…" and <Icon name="…"> in app/** and builds the Material
// Symbols Rounded subset (tools/icons-subset.py, fontTools) into dist/icons.json; exits 1 naming any icon the font
// lacks. node tools/icons-app.mjs [--src app] [--out dist/icons.json]
import { readdirSync, readFileSync, statSync, mkdirSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { join, dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { createRequire } from "node:module";
import { python } from "./python.mjs";

const root = join(dirname(fileURLToPath(import.meta.url)), "..");
const a = process.argv.slice(2);
const opt = (n, d) => { const i = a.indexOf(n); return i >= 0 ? a[i + 1] : d; };
const src = resolve(root, opt("--src", "app"));
const out = resolve(root, opt("--out", "dist/icons.json"));   // a build product, not firmware data (gen/ is checked by gen:check)

const PATTERNS = [/\bicon="([a-z0-9_]+)"/g, /\bicon:\s*"([a-z0-9_]+)"/g, /<Icon\s+name="([a-z0-9_]+)"/g];
// Quoted names inside name={…} / icon={…} expressions, and on lines marked // @icons (maps of icon names)
const EXPR = /\b(?:name|icon)=\{([^}]*)\}|\bicon:\s*([^,;}\n]+)/g;
const LITERAL = /"([a-z0-9_]+)"/g;

export function collectIcons(dir) {
  const names = new Set();
  const walk = (d) => {
    for (const f of readdirSync(d)) {
      const p = join(d, f);
      if (statSync(p).isDirectory()) walk(p);
      else if (/\.(tsx?|mjs)$/.test(f)) {
        const text = readFileSync(p, "utf8");
        for (const re of PATTERNS) for (const m of text.matchAll(re)) names.add(m[1]);
        for (const m of text.matchAll(EXPR)) for (const l of (m[1] ?? m[2]).matchAll(LITERAL)) names.add(l[1]);
        for (const line of text.split("\n")) if (line.includes("// @icons")) for (const l of line.split("// @icons")[0].matchAll(/:\s*"([a-z0-9_]+)"/g)) names.add(l[1]);
      }
    }
  };
  walk(dir);
  return [...names].sort();
}

if (process.argv[1] === fileURLToPath(import.meta.url)) {
  const names = collectIcons(src);
  try {   // the same names as last time: the subset is current (fontTools takes ~3 s on the 5 MB font)
    const prev = JSON.parse(readFileSync(out, "utf8"));
    if (JSON.stringify(Object.keys(prev.map).sort()) === JSON.stringify(names)) { console.log(`icons-app: ${names.length} icons, subset current`); process.exit(0); }
  } catch { /* none yet */ }
  const font = createRequire(import.meta.url).resolve("material-symbols/material-symbols-rounded.woff2");
  mkdirSync(dirname(out), { recursive: true });
  const r = spawnSync(python(), ["-I", join(root, "tools", "icons-subset.py"), font, out, ...names], { encoding: "utf8" });
  process.stdout.write(r.stdout ?? "");
  process.stderr.write(r.stderr ?? "");
  if (r.error) { console.log("icons-app: python3 with fontTools is needed (pip install fonttools brotli)"); process.exit(2); }
  process.exit(r.status ?? 1);
}

// npm run build: dist/index.html (Tauri frontendDist) with the web app's look: its tokens, base styles and icon font
// subset (gbs-control/public/tools/icons-app.mjs over these sources), and Preact from this package only.
import { build } from "esbuild";
import { readFileSync, writeFileSync, mkdirSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";

const ui = dirname(fileURLToPath(import.meta.url));
const web = join(ui, "..", "..", "gbs-control", "public");
const dist = join(ui, "dist");
mkdirSync(dist, { recursive: true });

// icons used by the flasher sources and by the web app components it imports (Icon, Logo)
const icons = spawnSync(process.execPath, [join(web, "tools", "icons-app.mjs"), "--src", join(ui, "src"), "--out", join(dist, "icons.json")], { stdio: "inherit" });
if (icons.status !== 0) { console.log("build: icon subset failed (npm ci in gbs-control/public, python3 with fontTools)"); process.exit(1); }
const iconData = JSON.parse(readFileSync(join(dist, "icons.json"), "utf8"));

const res = await build({
  entryPoints: [join(ui, "src", "main.tsx")], bundle: true, minify: true, format: "iife", target: ["es2021", "safari15"],
  write: false, jsx: "automatic", jsxImportSource: "preact", legalComments: "none", outdir: "out",
  alias: { preact: join(ui, "node_modules", "preact"), "@preact/signals": join(ui, "node_modules", "@preact", "signals") },
  plugins: [{
    name: "icons",
    setup(b) {
      b.onResolve({ filter: /^virtual:icons$/ }, () => ({ path: "icons", namespace: "virtual" }));
      b.onLoad({ filter: /.*/, namespace: "virtual" }, () => ({ contents: JSON.stringify(iconData.map), loader: "json" }));
    },
  }],
});
const js = res.outputFiles.find((f) => f.path.endsWith(".js")).text.replace(/<\/script/gi, "<\\/script");
const css = [readFileSync(join(web, "app", "styles", "tokens.css"), "utf8"), readFileSync(join(web, "app", "styles", "base.css"), "utf8"),
  `@font-face{font-family:"Material Symbols Rounded";font-style:normal;font-weight:400;font-display:block;src:url(${iconData.font}) format("woff2")}`,
  readFileSync(join(ui, "src", "styles.css"), "utf8")].join("\n");
const html = `<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>GBSC PRO Flasher</title><style>${css}</style></head><body><div id="app"></div><script>${js}</script></body></html>
`;
writeFileSync(join(dist, "index.html"), html);
console.log(`build: dist/index.html ${html.length} B`);

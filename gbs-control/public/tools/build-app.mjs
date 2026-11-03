// npm run build:app [-- --entry app/main.tsx --out dist/app.html]: one inlined HTML (+ .gz) with no external URL,
// then the size gate
import { build } from "esbuild";
import { readFileSync, writeFileSync, mkdirSync, existsSync } from "node:fs";
import { gzipSync } from "node:zlib";
import { join, dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { report, LIMIT } from "./size.mjs";
import { appDataPlugin } from "./app-data.mjs";
import { spawnSync } from "node:child_process";

const root = join(dirname(fileURLToPath(import.meta.url)), "..");
const a = process.argv.slice(2);
const opt = (n, d) => { const i = a.indexOf(n); return i >= 0 ? a[i + 1] : d; };
const entry = resolve(root, opt("--entry", "app/main.tsx"));
const out = resolve(root, opt("--out", "dist/app.html"));

// The icon subset first: an icon the font lacks fails the build here (tools/icons-app.mjs)
const icons = spawnSync(process.execPath, [join(root, "tools", "icons-app.mjs")], { stdio: "inherit" });
if (icons.status !== 0) { console.log("build-app: icon subset failed"); process.exit(1); }
const iconData = JSON.parse(readFileSync(join(root, "dist", "icons.json"), "utf8"));
const iconsPlugin = {
  name: "icons",
  setup(b) {
    b.onResolve({ filter: /^virtual:icons$/ }, () => ({ path: "icons", namespace: "virtual" }));
    b.onLoad({ filter: /.*/, namespace: "virtual" }, () => ({ contents: JSON.stringify(iconData.map), loader: "json" }));
  },
};

const res = await build({
  entryPoints: [entry], bundle: true, minify: true, format: "iife", target: ["es2020", "safari15"], write: false,
  jsx: "automatic", jsxImportSource: "preact", metafile: true, legalComments: "none", loader: { ".css": "text" },
  outdir: "out", plugins: [appDataPlugin, iconsPlugin],
});
const js = res.outputFiles.find((f) => f.path.endsWith(".js")).text;
const tplPath = join(dirname(entry), "index.html.tpl");
const tpl = readFileSync(existsSync(tplPath) ? tplPath : join(root, "app", "index.html.tpl"), "utf8");
// Stylesheet: the tokens, the app styles, the icon font, then a page's own styles.css if any
const part = (p) => (existsSync(p) ? readFileSync(p, "utf8") : "");
const css = [part(join(root, "app", "styles", "tokens.css")), part(join(root, "app", "styles", "base.css")),
  `@font-face{font-family:"Material Symbols Rounded";font-style:normal;font-weight:400;font-display:block;src:url(${iconData.font}) format("woff2")}`,
  part(join(dirname(entry), "styles.css"))].join("\n");
const html = tpl.replace("/*__CSS__*/", () => css).replace("/*__JS__*/", () => js.replace(/<\/script/gi, "<\\/script"));
mkdirSync(dirname(out), { recursive: true });
writeFileSync(out, html);
writeFileSync(out + ".gz", gzipSync(Buffer.from(html), { level: 9 }));
writeFileSync(out.replace(/\.html$/, ".meta.json"), JSON.stringify(res.metafile));
const r = report(out, LIMIT);
console.log(`build-app: ${out} ${html.length} B, gzip ${r.bytes} B of ${LIMIT} B`);
if (r.bytes > LIMIT && !a.includes("--no-gate")) { console.log("build-app: over the size limit"); process.exit(1); }

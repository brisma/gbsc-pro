// npm run parity [-- --gen <dir>] [--exclusions <file>]
// Every setting (registry), menu node, action and route of gen/ is used by the web app (app/), or is an exclusion
// with a reason in test/parity-exclusions.json. No hand-kept list of tests. A menu node counts as
// used when its id is in the app's sources (also through the m()/sys()/dv() helpers of layout/pages.ts) or a node
// above it below the page level is; a setting or action when a used node binds it or its name is in the sources; a
// route when its path is. Menu section headers have no function of their own.
import { readFileSync, readdirSync, statSync, existsSync } from "node:fs";
import { join, dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const pub = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const a = process.argv.slice(2);
const opt = (n, d) => { const i = a.indexOf(n); return i >= 0 ? a[i + 1] : d; };
const genDir = resolve(opt("--gen", join(pub, "gen")));
const exFile = resolve(opt("--exclusions", join(pub, "test", "parity-exclusions.json")));
const json = (f) => JSON.parse(readFileSync(f, "utf8"));

function sources(dir) {
  let out = "";
  for (const name of readdirSync(dir)) {
    const p = join(dir, name);
    if (statSync(p).isDirectory()) out += sources(p);
    else if (/\.(ts|tsx|tpl)$/.test(name)) out += readFileSync(p, "utf8") + "\n";
  }
  return out;
}
const src = sources(join(pub, "app"));
const quoted = (s) => src.includes(`"${s}"`) || src.includes(`\`${s}\``) || src.includes(`'${s}'`);
const HELPERS = [["system/", "sys"], ["developer/", "dv"]];
function idUsed(id) {
  if (quoted(id) || quoted(`menu:${id}`) || src.includes(`"menu:${id}~`) || src.includes(`m("${id}`)) return true;
  for (const [prefix, fn] of HELPERS)
    if (id.startsWith(prefix) && src.includes(`${fn}("${id.slice(prefix.length)}`)) return true;
  return false;
}

// a path in the sources, also with one segment built (`/api/adv/${kind}/delete`); /api/actions/<name>: the action
function routeUsed(path) {
  if (src.includes(path)) return true;
  if (path.startsWith("/api/actions/")) return used.has(`action:${path.slice("/api/actions/".length)}`) || src.includes("/api/actions/${");
  const seg = path.split("/");
  const esc = (s) => s.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
  for (let i = 1; i < seg.length; i++) {
    const re = new RegExp(seg.map((s, j) => (j === i ? "\\$\\{[^}]+\\}" : esc(s))).join("/"));
    if (re.test(src)) return true;
  }
  return false;
}
const used = new Set(), expected = new Map();
const menu = json(join(genDir, "menu.json"));
const usedChoices = new Set();
function walk(n, parentUsed) {
  for (const c of n.children || []) {
    const deep = c.id.includes("/");
    const isUsed = c.type === "header" || idUsed(c.id) || (deep && parentUsed);
    expected.set(`menu:${c.id}`, c.type);
    if (isUsed) {
      used.add(`menu:${c.id}`);
      if (c.setting) used.add(`setting:${c.setting}`);
      for (const k of ["action", "action_inc"]) if (c[k]) used.add(`action:${c[k]}`);
      if (c.choice) usedChoices.add(c.choice);
      for (const act of Object.values((c.adjust && menu.adjust?.[c.adjust]) || {})) used.add(`action:${act}`);   // the Screen pad
    }
    walk(c, isUsed && deep);
  }
}
walk(menu.root, false);
// the Quick menu mirrors settings and choices of the pages: used when what it binds is used there
for (const c of menu.quick.children || []) {
  expected.set(`menu:${c.id}`, c.type);
  if (idUsed(c.id) || (c.setting && used.has(`setting:${c.setting}`)) || (c.choice && usedChoices.has(c.choice))) used.add(`menu:${c.id}`);
}
for (const s of json(join(genDir, "registry.json")).settings) {
  expected.set(`setting:${s.key}`, "setting");
  if (quoted(s.key) || quoted(`setting:${s.key}`)) used.add(`setting:${s.key}`);
}
for (const x of json(join(genDir, "actions-all.json"))) {
  expected.set(`action:${x.name}`, "action");
  if (quoted(x.name) || quoted(`action:${x.name}`) || src.includes(`/api/actions/${x.name}`)) used.add(`action:${x.name}`);
}
const routes = json(join(genDir, "routes.json"));
for (const r of [...routes.normal, ...routes.safe]) for (const m of r.methods) {
  expected.set(`route:${m} ${r.path}`, "route");
  if (routeUsed(r.path)) used.add(`route:${m} ${r.path}`);
}

const excluded = existsSync(exFile) ? json(exFile) : [];
const problems = [];
const exRefs = new Set(excluded.map((x) => x.ref));
for (const x of excluded) {
  if (!x.reason) problems.push(`exclusion without a reason: ${x.ref}`);
  if (!expected.has(x.ref)) problems.push(`stale exclusion: ${x.ref}`);
  else if (used.has(x.ref)) problems.push(`exclusion no longer needed (the app uses it): ${x.ref}`);
}
for (const id of expected.keys()) if (!used.has(id) && !exRefs.has(id)) problems.push(`not in the web app: ${id}`);

console.log(`parity: covered ${used.size}, excluded ${excluded.length}, problems ${problems.length}`);
for (const p of problems) console.log(p);
process.exit(problems.length ? 1 : 0);

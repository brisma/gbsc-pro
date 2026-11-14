// The flasher bundles the whole web app stylesheet: its own classes must not share a name with the web app's
import { test } from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";

const src = join(dirname(fileURLToPath(import.meta.url)), "..", "src");
// the web app's classes the flasher uses on purpose, and state modifiers that only mean something next to a class
const shared = new Set(["ui", "flasher", "hd", "ttl", "btn", "pri", "ghost", "ms", "mono", "drop", "field", "warnbox",
  "wide", "big", "on", "no", "done", "now", "wait"]);
const own = (name: string) => name.startsWith("f-") || shared.has(name);

test("the stylesheet's own classes carry the f- prefix", () => {
  const css = readFileSync(join(src, "styles.css"), "utf8").replace(/\/\*[\s\S]*?\*\//g, "").replace(/\{[^}]*\}/g, "{}");
  const names = [...css.matchAll(/\.([a-zA-Z][\w-]*)/g)].map((m) => m[1]);
  assert.ok(names.length > 40);
  assert.deepEqual([...new Set(names.filter((n) => !own(n)))], []);
});

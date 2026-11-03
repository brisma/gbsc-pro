// What the page embeds of gen/: a trimmed registry and action list (size budget)
import { test } from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { join } from "node:path";
import { trimRegistry, trimActions } from "../../tools/app-data.mjs";
import { publicDir } from "./sim-helpers.ts";

test("registry is trimmed to what the page reads", () => {
  const full = readFileSync(join(publicDir, "gen", "registry.json"), "utf8");
  const t = JSON.parse(trimRegistry(full));
  const e = t.settings.find((x: any) => x.key === "input.source");
  assert.deepEqual(Object.keys(e).sort(), ["caps", "def", "key", "max", "min", "options", "type"]);
  assert.deepEqual(e.options[5], { n: "S-Video", v: 5, caps: 1 });
  assert.equal(t.settings.find((x: any) => x.key === "output.preset").bridged, true);
  assert.equal(t.settings.find((x: any) => x.key === "adv.format_sv").options.find((o: any) => o.v === 11).advanced, true);
  assert.ok(trimRegistry(full).length < full.length * 0.6);
  assert.equal(t.boolWords, undefined);
});

test("actions are trimmed to name, caps and flags", () => {
  const full = readFileSync(join(publicDir, "gen", "actions-all.json"), "utf8");
  const t = JSON.parse(trimActions(full));
  assert.deepEqual(t.find((a: any) => a.name === "system.restart"), { name: "system.restart", caps: 0, flags: ["confirm"] });
});

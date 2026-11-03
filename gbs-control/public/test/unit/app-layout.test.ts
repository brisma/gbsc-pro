// Page description, row resolution and the menu binding table
import { test } from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { join } from "node:path";
import { PAGES } from "../../app/layout/pages.ts";
import { BINDINGS, bindingFor } from "../../app/layout/bindings.ts";
import { refExists, rowState } from "../../app/rows/resolve.ts";
import { allNodes, nodeById, optionsFor } from "../../app/model/menu.ts";
import { BOARD_CAPS, type PredState } from "../../app/model/predicates.ts";
import { formatLive, formatSetting } from "../../app/model/format.ts";
import { publicDir } from "./sim-helpers.ts";

const registry = JSON.parse(readFileSync(join(publicDir, "gen", "registry.json"), "utf8"));

function state(over: Partial<PredState> & { values?: Record<string, number> } = {}): PredState {
  const values: Record<string, number> = {};
  for (const e of registry.settings) if (e.type !== "str") values[e.key] = e.def ?? 0;
  return {
    caps: BOARD_CAPS.pro, input: 1, menuMode: 1, devmode: false, profileActive: false, wifiAp: false,
    signal: { noSignal: false, interlaced: false, scanlinesOk: true, advNtsc: false, updateAvailable: false, updateManual: false,
              advControllerUpdate: false, presetId: 1, srcHz10: 600 },
    ...over, values: { ...values, ...(over.values ?? {}) },
  };
}

test("every row ref exists in gen", () => {
  let n = 0;
  for (const p of PAGES) for (const s of p.sections) for (const g of s.groups ?? []) for (const r of g.rows) {
    if (r.ref) { assert.ok(refExists(r.ref), `${p.id}/${s.id}: ${r.ref}`); n++; }
  }
  assert.ok(refExists("menu:input/source"));
  assert.ok(refExists("setting:adv.brightness"));
  assert.ok(refExists("action:system.restart"));
  assert.ok(refExists("live:adc_gain"));
  assert.ok(refExists("text:wifi.hostname"));
  assert.ok(!refExists("menu:input/nope"));
  assert.ok(!refExists("setting:nope.key"));
  assert.ok(!refExists("action:nope"));
  assert.ok(!refExists("live:nope"));
  assert.ok(n >= 0);
});

test("every menu node has a binding", () => {
  const nodes = allNodes().filter((x) => x.id !== "" && x.type !== "header");
  for (const node of nodes) {
    const b = bindingFor(node.id);
    assert.ok(b, `no binding for ${node.id}`);
    if (b!.web === "excluded") assert.ok(b!.reason && b!.reason.length > 10, `excluded without a reason: ${node.id}`);
  }
  assert.equal(BINDINGS.length, nodes.length);
  const byFn = (fn: string) => BINDINGS.filter((b) => nodeById(b.id)?.fn === fn);
  assert.ok(byFn("tv_osd").every((b) => b.web === "excluded"));
  assert.ok(byFn("open_info").every((b) => b.web === "status-page"));
  assert.ok(byFn("wifi_reset").every((b) => b.web === "wifi-page"));
  assert.ok(byFn("check_update").every((b) => b.web === "update-page"));
  assert.ok(byFn("install_manual").every((b) => b.web === "update-page"));
  assert.ok(byFn("edit_hosts").every((b) => b.web === "wifi-page"));
  assert.ok(byFn("edit_ota").every((b) => b.web === "row"));
  assert.ok(byFn("quick_profile").every((b) => b.web === "row"));
  assert.equal(bindingFor("system/developer-menu")?.web, "mode-switch");
  assert.equal(bindingFor("picture/scanline-strength")?.web, "scanline-cards");
  assert.equal(bindingFor("profiles/load")?.web, "profiles-page");
});

test("rows hidden by cable, greyed by setting", () => {
  const std = { kind: "inline" as const, ref: "menu:input/video-standard~adv.format_sv" as const, icon: "public" };
  assert.equal(rowState(std, state({ input: 1 })).visible, false);              // RGB SCART: hidden as on the TV
  assert.equal(rowState(std, state({ input: 5 })).visible, true);
  const smooth = { kind: "toggle" as const, ref: "menu:input/2x-smooth" as const, icon: "blur_on" };
  assert.deepEqual(rowState(smooth, state({ input: 5, values: { "adv.i2p": 0 } })), { visible: true, reason: "Turn 2X on first" });
  assert.deepEqual(rowState(smooth, state({ input: 5, values: { "adv.i2p": 1 } })), { visible: true, reason: null });
  assert.equal(rowState(smooth, state({ input: 5, menuMode: 0 })).visible, false);   // an Advanced node in Easy
  const web = { kind: "toggle" as const, ref: "setting:adv.brightness" as const, icon: "x", advanced: true };
  assert.equal(rowState(web, state({ menuMode: 0 })).visible, false);
  const adv = { kind: "toggle" as const, ref: "setting:output.frame_lock" as const, icon: "x", cap: "adv" as const };
  assert.equal(rowState(adv, state({ caps: BOARD_CAPS.standard })).visible, false);
  assert.equal(rowState({ kind: "action", ref: "action:dev.tv5725_read", icon: "x" }, state()).visible, false);   // dev action outside Dev
});

test("a row inside a sub follows the sub's conditions as on the TV", () => {
  const dec = { kind: "stepper" as const, ref: "menu:input/decoder/brightness" as const, icon: "x" };
  assert.equal(rowState(dec, state({ input: 1 })).visible, false);              // Decoder: S-Video/Composite only
  assert.equal(rowState(dec, state({ input: 5 })).visible, true);
  assert.equal(rowState(dec, state({ input: 5, menuMode: 0 })).visible, false); // and Advanced only
});

test("values formatted as on the TV", () => {
  assert.equal(formatLive("position", [40, 7]), "40/7");
  assert.equal(formatLive("sdram", 3), "162MHz");
  assert.equal(formatLive("out_rate", 600), "60Hz");
  assert.equal(formatLive("freeze", 1), "On");
  assert.equal(formatSetting(nodeById("system/sound/volume")!, 40), "40%");
  assert.equal(formatSetting(nodeById("input/source")!, 5), "S-Video");
});

test("analog out reads as the signal it sends, not On and Off", () => {
  const n = nodeById("output/analog-out")!;
  assert.deepEqual(optionsFor(n, { mode: "advanced", caps: 0, current: 0, profileActive: false }), [{ n: "RGB", v: 0 }, { n: "YPbPr", v: 1 }]);
  assert.equal(formatSetting(n, 0), "RGB");
  assert.equal(formatSetting(n, 1), "YPbPr");
  const row = PAGES.flatMap((p) => p.sections).flatMap((s) => s.groups ?? []).flatMap((g) => g.rows).find((r) => r.ref === "menu:output/analog-out")!;
  assert.equal(row.kind, "inline");
});

test("decoder levels say they act before Picture, on S-Video and Composite", async () => {
  const { helpFor } = await import("../../app/texts/help.ts");
  for (const k of ["brightness", "contrast", "saturation", "hue"]) {
    const h = helpFor(`input/decoder/${k}`, undefined);
    assert.ok(h, k);
    assert.match(h![1], /before Picture/);
    assert.match(h![1], /S-Video and Composite/);
  }
  assert.match(nodeById("input/decoder/brightness")!.help, /before Picture/);   // the TV and OLED line
});

test("Settings follow the signal chain and place every row once", async () => {
  const settings = PAGES.find((p) => p.id === "settings")!;
  assert.deepEqual(settings.sections.map((s) => s.id), ["input", "scaler", "picture", "output"]);
  const where = new Map<string, string>();
  for (const s of settings.sections) for (const g of s.groups ?? []) {
    assert.ok(g.title, `${s.id}: a group without a title`);
    for (const r of g.rows) if (r.ref) { assert.ok(!where.has(r.ref), `${r.ref} is in ${where.get(r.ref)} and in ${s.id}`); where.set(r.ref, s.id); }
  }
  assert.equal(where.get("menu:input/source"), "input");
  assert.equal(where.get("menu:output/smooth-scrolling"), "scaler");
  assert.equal(where.get("menu:picture/deinterlace"), "scaler");
  assert.equal(where.get("menu:screen/position"), "scaler");
  assert.equal(where.get("menu:screen/pal-full-height"), "scaler");
  assert.equal(where.get("menu:picture/filter"), "picture");
  assert.equal(where.get("menu:output/resolution"), "output");
  assert.equal(where.get("menu:output/hdmi-range"), "output");
  assert.equal(where.size, 64);   // the rows of the former Input, Output, Picture and Screen sections, none lost
});

test("old section bookmarks land on the new pages", async () => {
  const { parseHash } = await import("../../app/shell/router.ts");
  assert.deepEqual(parseHash("#/settings/screen"), { page: "settings", section: "scaler", id: null });
  assert.deepEqual(parseHash("#/settings/output"), { page: "settings", section: "output", id: null });
  assert.deepEqual(parseHash("#/status/signal"), { page: "status", section: null, id: null });
  assert.deepEqual(parseHash("#/settings"), { page: "settings", section: "input", id: null });
});

// The page's port of menuVisible/menuEnabled against the truth table printed by the firmware
import { test } from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { join } from "node:path";
import { visible, disabledReason, stateFromStore, BOARD_CAPS, type PredState } from "../../app/model/predicates.ts";
import { allNodes } from "../../app/model/menu.ts";
import { publicDir } from "./sim-helpers.ts";

const table = JSON.parse(readFileSync(join(publicDir, "gen", "predicates.json"), "utf8"));
const registry = JSON.parse(readFileSync(join(publicDir, "gen", "registry.json"), "utf8"));

function stateOf(sc: any): PredState {
  const caps = BOARD_CAPS[sc.state.board as "pro" | "standard"];
  const values: Record<string, number> = {};
  for (const e of registry.settings) if (e.type !== "str") values[e.key] = e.def ?? 0;
  values["input.source"] = sc.state.input;
  Object.assign(values, sc.state.values);
  const g = sc.state.signal;
  return {
    caps, input: sc.state.input, menuMode: sc.state.menu_mode, devmode: sc.state.devmode,
    profileActive: sc.state.profile_active >= 0, wifiAp: sc.state.wifi_ap, values,
    signal: { noSignal: g.no_signal, interlaced: g.interlaced, scanlinesOk: g.scanlines_ok, advNtsc: g.adv_ntsc,
              updateAvailable: g.update_available, updateManual: g.update_manual, advControllerUpdate: g.adv_controller_update,
              presetId: g.preset_id, srcHz10: g.src_hz10 },
  };
}

test("predicates match the firmware truth table", () => {
  const nodes = allNodes().filter((n) => n.id !== "" && !n.id.startsWith("quick/"));
  assert.ok(nodes.length > 150);
  let checked = 0;
  for (const sc of table.scenarios) {
    const s = stateOf(sc);
    const want = new Set<string>(sc.visible);
    for (const n of nodes) {
      assert.equal(visible(n, s), want.has(n.id), `${sc.name}: visible ${n.id}`);
      assert.equal(disabledReason(n, s), sc.disabled[n.id] ?? null, `${sc.name}: reason ${n.id}`);
      checked++;
    }
  }
  assert.ok(checked > 4000);
});

// While Auto searches, the hardware walks the paths every few seconds: the rows that depend on the input follow the last
// found one (input.last), or every probe moved the page
test("state from the store keeps the last found input while Auto searches", () => {
  const base = { values: { "input.source": 0 }, wifiMode: "STA", mode: "advanced" as const, profileActive: false };
  const s = stateFromStore({ ...base, info: { caps: 63, input: { active: 5, last: 1, state: "searching" }, signal: { none: 1 } } });
  assert.equal(s.input, 1);
  const f = stateFromStore({ ...base, info: { caps: 63, input: { active: 4, last: 1, state: "found" }, signal: { none: 0 } } });
  assert.equal(f.input, 4);
});

test("state from the store reads info, wifi and the browser mode", () => {
  const s = stateFromStore({
    values: { "input.source": 0, "system.devmode": 1 },
    info: { caps: 63, input: { active: 5 }, signal: { none: 0, interlaced: 1, scanlines_ok: 0, adv_ntsc: 1, preset: 0x11, src_hz10: 599 },
            update: { offer: { version: "3.0.1" }, manual: { version: "" } }, adv_controller: { update_available: true } },
    wifiMode: "AP", mode: "advanced", profileActive: true,
  });
  assert.equal(s.input, 5);
  assert.equal(s.menuMode, 1);
  assert.equal(s.devmode, true);
  assert.equal(s.wifiAp, true);
  assert.deepEqual(s.signal, { noSignal: false, interlaced: true, scanlinesOk: false, advNtsc: true, updateAvailable: true,
                               updateManual: false, advControllerUpdate: true, presetId: 0x11, srcHz10: 599 });
  const e = stateFromStore({ values: {}, info: null, wifiMode: "", mode: "easy", profileActive: false });
  assert.equal(e.menuMode, 0);
  assert.equal(e.signal.noSignal, true);   // nothing known yet: as without a signal
});

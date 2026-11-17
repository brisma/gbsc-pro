// The simulator writes and reads files as the firmware does: option ids, on/off, schema 2, the named profile header
import { test, before, after } from "node:test";
import assert from "node:assert/strict";
import { startSim, type Sim } from "../../sim/server.ts";
import { fixturePage } from "./sim-helpers.ts";
import { backupText } from "../../sim/routes-system.ts";

let sim: Sim;
before(async () => { sim = await startSim({ port: 0, wsPort: 0, pagePath: fixturePage }); });
after(async () => { await sim.stop(); });

test("a backup holds ids and schema 2", () => {
  sim.state.values.set("output.aspect", 1);
  sim.state.values.set("picture.scanlines", 0);
  const t = backupText(sim);
  assert.match(t, /\[file cfg\/device\.cfg\]\nschema=2\n/);
  assert.match(t, /\noutput\.aspect=force_4_3\n/);
  assert.match(t, /\npicture\.scanlines=off\n/);
});

test("a profile export and import keep settings and fingerprint", () => {
  sim.profiles.slots[0] = { name: "SNES", region: "None", hasFp: true, fpInput: 1, fpLines: 262, lastused: 0, modes: ["ntsc"],
                            settings: { "picture.deinterlace": 1, "audio.volume": 70 } };
  const text = sim.profiles.exportText(0);
  assert.match(text, /^schema=2\n/);
  assert.match(text, /\nprofile\.fp\.source=rgb_scart\n/);
  assert.match(text, /\npicture\.deinterlace=bob\n/);
  assert.doesNotMatch(text, /profile\.fp\.input/);
  sim.profiles.restore(new Map([[0, text]]), null);
  const back = sim.profiles.slots[0]!;
  assert.equal(back.hasFp, true);
  assert.equal(back.fpInput, 1);
  assert.equal(back.fpLines, 262);
  assert.deepEqual(back.settings, { "picture.deinterlace": 1, "audio.volume": 70 });
});

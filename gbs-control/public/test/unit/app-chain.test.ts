// The signal chain model: stages, texts and cable states
import { test } from "node:test";
import assert from "node:assert/strict";
import { chain, outputLines, upscaleText, scanlinesText, type ChainIn } from "../../app/model/chain.ts";

const base: ChainIn = { offline: false, searching: false, signal: { none: 0, lines: 240, interlaced: 0, src_hz10: 600, out_hz10: 600 },
  cable: "RGB SCART", resolution: "960p/1024p", filter: "Normal", scanlines: "Off", frameLock: false, ypbpr: false };

test("a stable signal: four stages, three green cables", () => {
  const c = chain(base);
  assert.equal(c.state, "ok");
  assert.deepEqual(c.cables, ["ok", "ok", "ok"]);
  assert.deepEqual(c.stages.map((s) => [s.name, s.value, s.detail]), [
    ["Input", "RGB SCART", "240p 60 Hz"], ["Scaler", "Upscale ×4", "Frame lock off"],
    ["Picture", "Normal filter", "Scanlines off"], ["Output", "960p", "60.0 Hz, RGB"]]);
  assert.ok(c.stages.every((s) => !s.dim && !s.tone));
});

test("a 50 Hz source on 960p/1024p goes out at 1024p", () => {
  const c = chain({ ...base, signal: { none: 0, lines: 288, interlaced: 0, src_hz10: 500, out_hz10: 500 } });
  assert.equal(c.stages[3].value, "1024p");
  assert.equal(c.stages[1].value, "Upscale ×3.6");
  assert.equal(c.stages[0].detail, "288p 50 Hz");
});

test("interlaced input, non-integer factor, frame lock, YPbPr, scanlines", () => {
  const c = chain({ ...base, signal: { none: 0, lines: 480, interlaced: 1, src_hz10: 599, out_hz10: 599 }, resolution: "1080p", frameLock: true, ypbpr: true, scanlines: "30", filter: "Sharp" });
  assert.deepEqual(c.stages.map((s) => [s.value, s.detail]), [["RGB SCART", "480i 60 Hz"], ["Upscale ×2.3", "Frame lock on"], ["Sharp filter", "Scanlines 30"], ["1080p", "59.9 Hz, YPbPr"]]);
});

test("searching: the first cable searches, the rest is off, the output is dimmed", () => {
  const c = chain({ ...base, searching: true, signal: { none: 1 } });
  assert.equal(c.state, "search");
  assert.deepEqual(c.cables, ["search", "off", "off"]);
  assert.deepEqual([c.stages[0].detail, c.stages[0].tone], ["trying…", "warn"]);
  assert.equal(c.stages[1].value, "Upscale");
  assert.deepEqual([c.stages[3].value, c.stages[3].detail, c.stages[3].dim], ["960p/1024p", "RGB", true]);
});

test("no signal and offline", () => {
  const lost = chain({ ...base, signal: { none: 1 } });
  assert.equal(lost.state, "lost");
  assert.deepEqual(lost.cables, ["off", "off", "off"]);
  assert.deepEqual([lost.stages[0].detail, lost.stages[0].tone], ["no signal", "bad"]);
  const off = chain({ ...base, offline: true });
  assert.equal(off.state, "off");
  assert.equal(off.stages[0].detail, "not connected");
  assert.equal(off.stages[0].tone, undefined);
  assert.equal(chain({ ...base, signal: null }).state, "lost");
});

test("helpers", () => {
  assert.equal(outputLines("960p/1024p", 600), 960);
  assert.equal(outputLines("960p/1024p", 500), 1024);
  assert.equal(outputLines("480p/576p", 0), 480);
  assert.equal(outputLines("1200p", 500), 1200);
  assert.equal(outputLines("960p fixed", 500), 960);
  assert.equal(outputLines("Custom", 600), null);
  assert.equal(upscaleText(240, 1200), "Upscale ×5");
  assert.equal(upscaleText(0, 960), "Upscale");
  assert.equal(upscaleText(240, null), "Upscale");
  assert.equal(scanlinesText("Off"), "Scanlines off");
  assert.equal(scanlinesText("Light"), "Light scanlines");
  assert.equal(scanlinesText("40"), "Scanlines 40");
  assert.equal(scanlinesText("Custom"), "Custom scanlines");
});

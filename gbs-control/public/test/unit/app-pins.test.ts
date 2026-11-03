// Quick pins: per browser, default = the menu's Quick items
import { test } from "node:test";
import assert from "node:assert/strict";
import { defaultPins, cleanPins, addPin, removePin, movePin, rowForNode, pinnable } from "../../app/pages/quick/pins.ts";
import { menu, nodeById } from "../../app/model/menu.ts";

test("pins default to the quick menu", () => {
  const quick = (menu.quick.children ?? []).map((n) => n.id).filter((id) => id !== "quick/tv-osd");   // excluded binding
  assert.deepEqual(defaultPins(), quick);
  assert.ok(defaultPins().includes("quick/profile"));
});

test("pins add, remove, reorder and drop unknown ids", () => {
  let p = defaultPins();
  p = addPin(p, "picture/brightness");
  assert.equal(p.at(-1), "picture/brightness");
  assert.deepEqual(addPin(p, "picture/brightness"), p);                    // no duplicate
  p = removePin(p, "quick/filter");
  assert.ok(!p.includes("quick/filter"));
  const i = p.indexOf("picture/brightness");
  p = movePin(p, "picture/brightness", -1);
  assert.equal(p.indexOf("picture/brightness"), i - 1);
  assert.deepEqual(movePin(p, p[0], -1), p);                              // the first stays first
  assert.deepEqual(cleanPins(["nope/x", "picture/brightness", "quick/tv-osd", "picture/brightness", 7 as unknown as string]),
                   ["picture/brightness"]);
  assert.deepEqual(cleanPins("garbage" as unknown as string[]), defaultPins());
});

test("rows for pinned nodes", () => {
  assert.equal(rowForNode(nodeById("quick/source")!)!.kind, "picker");
  assert.equal(rowForNode(nodeById("quick/resolution")!)!.kind, "picker");
  assert.equal(rowForNode(nodeById("quick/filter")!)!.look, "filter");
  assert.equal(rowForNode(nodeById("picture/brightness")!)!.kind, "stepper");
  assert.equal(rowForNode(nodeById("output/always-60hz")!)!.kind, "toggle");
  assert.equal(rowForNode(nodeById("output/hdmi-range")!)!.kind, "inline");
  assert.equal(rowForNode(nodeById("screen/position")!)!.kind, "pad");
  assert.ok(pinnable(nodeById("picture/brightness")!));
  assert.ok(!pinnable(nodeById("picture/reset-picture")!));               // actions are not offered
  assert.ok(!pinnable(nodeById("system/sound/volume")!));                 // only Settings sections
});

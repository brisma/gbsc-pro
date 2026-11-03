// Status: event texts, console lines (merge by stamp, filters, pause), relative times
import { test } from "node:test";
import assert from "node:assert/strict";
import { eventText, presetName, ago } from "../../app/texts/events.ts";
import { mergeLog } from "../../app/store/device.ts";
import { consoleLines, CATEGORIES, issueReport } from "../../app/pages/status/console-model.ts";

const ctx = { inputName: (n: number) => ["", "RGB SCART", "RGB SoG", "VGA", "Component", "S-Video", "Composite"][n] ?? "?",
              profileName: (id: string) => ({ p03: "Mega Drive · PAL" } as Record<string, string>)[id] ?? "" };

test("event texts for every type", () => {
  const types = ["input_found", "input_lost", "input_changed", "profile_loaded", "profile_saved", "resolution", "update_offered", "update_done"];
  for (const t of types) {
    const e = eventText({ seq: 1, ms: 0, type: t, a: 1, b: 0 }, ctx);
    assert.ok(e.title.length > 4 && e.icon.length > 1, t);
  }
  assert.equal(eventText({ seq: 1, ms: 0, type: "input_found", a: 1, b: 0 }, ctx).title, "Input found: RGB SCART");
  assert.match(eventText({ seq: 1, ms: 0, type: "input_found", a: 6, b: 1 }, ctx).sub, /fallback/);
  assert.equal(eventText({ seq: 1, ms: 0, type: "input_lost", a: 6, b: 0 }, ctx).title, "Signal lost on Composite");
  assert.match(eventText({ seq: 1, ms: 0, type: "input_changed", a: 5, b: 1 }, ctx).sub, /by hand/);
  assert.equal(eventText({ seq: 1, ms: 0, type: "profile_loaded", a: 3, b: 0 }, ctx).title, "Profile loaded: Mega Drive · PAL");
  assert.equal(eventText({ seq: 1, ms: 0, type: "profile_saved", a: 9, b: 0 }, ctx).title, "Profile saved: p09");   // gone since
  assert.equal(eventText({ seq: 1, ms: 0, type: "resolution", a: 0x15, b: 1 }, ctx).title, "Resolution: 1080p");
  assert.match(eventText({ seq: 1, ms: 0, type: "resolution", a: 0x15, b: 1 }, ctx).sub, /PAL source.*RGB SCART/);
  assert.match(eventText({ seq: 1, ms: 0, type: "update_offered", a: 1, b: 0 }, ctx).title, /Beta/);
  assert.match(eventText({ seq: 1, ms: 0, type: "update_done", a: 0, b: 0 }, ctx).sub, /upload/);
  assert.equal(eventText({ seq: 1, ms: 0, type: "nope", a: 0, b: 0 }, ctx).title, "nope");
  assert.equal(presetName(0x04), "480p"); assert.equal(presetName(0x14), "576p"); assert.equal(presetName(0x21), "Bypass");
});

test("console drops lines it already has by stamp", () => {
  const base = [{ t: 1000, c: "sys", m: "boot", l: "info", s: 1 }];
  const merged = mergeLog(base, [{ t: 1000, c: "sys", m: "boot" }, { t: 1200, c: "input", m: "found RGB SCART" }], false, 400);
  assert.deepEqual(merged.map((e) => e.m), ["boot", "found RGB SCART"]);
  assert.equal(mergeLog(merged, [{ t: 5, c: "sys", m: "new boot" }], true, 400).length, 1);   // a restart starts over
  const many = Array.from({ length: 500 }, (_, i) => ({ t: i, c: "gbs", m: `l${i}` }));
  assert.equal(mergeLog([], many, false, 400).length, 400);                                     // the newest kept
});

test("console filter, debug lines and pause", () => {
  const lines = [{ t: 1, c: "sys", m: "a", l: "info" }, { t: 2, c: "input", m: "b", l: "debug" }, { t: 3, c: "adv", m: "c" }, { t: 4, c: "sys", m: "d" }];
  assert.deepEqual(consoleLines(lines, { cat: "all", debug: true, pausedAt: null }).map((x) => x.m), ["a", "b", "c", "d"]);
  assert.deepEqual(consoleLines(lines, { cat: "all", debug: false, pausedAt: null }).map((x) => x.m), ["a", "c", "d"]);   // l: lines have no level
  assert.deepEqual(consoleLines(lines, { cat: "sys", debug: true, pausedAt: null }).map((x) => x.m), ["a", "d"]);
  assert.deepEqual(consoleLines(lines, { cat: "all", debug: true, pausedAt: 2 }).map((x) => x.m), ["a", "b"]);         // paused after 2 lines
  assert.ok(CATEGORIES.includes("profile") && CATEGORIES.includes("menu") && CATEGORIES.length === 13);
});

test("relative times", () => {
  assert.equal(ago(10000, 10500), "now");
  assert.equal(ago(10000, 22000), "12 s ago");
  assert.equal(ago(0, 3 * 60000 + 5000), "3 min ago");
  assert.equal(ago(0, 2 * 3600000 + 60000), "2 h ago");
});

test("copy for an issue has versions, board, crash and the last lines", () => {
  const md = issueReport({ fw: "3.0.0-dev0", board: "pro", adv_controller: { version: "3.0.0" }, crash: { cause: 28, epc1: "40201234", epc2: "00000000", epc3: "00000000", excvaddr: "00000000", depc: "00000000", pcs: "4020AAAA 4020BBBB" } },
                         Array.from({ length: 60 }, (_, i) => ({ t: i * 1000, c: "gbs", m: `line ${i}` })));
  assert.match(md, /GBS-Control 3\.0\.0-dev0/);
  assert.match(md, /ADV Controller 3\.0\.0/);
  assert.match(md, /Board: pro/);
  assert.match(md, /40201234/);
  assert.match(md, /4020AAAA 4020BBBB/);   // the stack PCs for addr2line
  assert.match(md, /line 59/);
  assert.doesNotMatch(md, /line 9\b/);   // the last 50 lines only
});

test("copy for an issue leaves the WiFi network's name out", () => {
  const lines = [{ t: 3708, c: "wifi", m: "(WiFi): Connected to Casa Rossi" },
                 { t: 5108, c: "wifi", m: "connected to Casa Rossi (192.168.1.20, -69 dBm, channel 11)" }];
  const md = issueReport({ fw: "3.0.0", board: "pro", wifi: { ssid: "Casa Rossi" } }, lines);
  assert.doesNotMatch(md, /Casa Rossi/);   // the report goes into a public issue
  assert.match(md, /connected to \(network\) \(192\.168\.1\.20, -69 dBm, channel 11\)/);
  assert.match(issueReport({ fw: "3.0.0", board: "pro", wifi: { ssid: "" } }, lines), /Casa Rossi/);   // no name known: lines as they are
});

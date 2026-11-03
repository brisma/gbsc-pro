// Page model from the firmware data: options, choices, scanlines, columns, versions
import { test } from "node:test";
import assert from "node:assert/strict";
import { gzipSync } from "node:zlib";
import { nodeById, labelFor, optionsFor } from "../../app/model/menu.ts";
import { choiceValue, choiceWrites, choiceOptions } from "../../app/model/choices.ts";
import { scanlineDarkness, scanlineOptions, scanlineValue, scanlineWrites } from "../../app/model/scanlines.ts";
import { balance } from "../../app/model/columns.ts";
import { compareVersions, readImageVersion } from "../../app/model/version.ts";
import { BOARD_CAPS } from "../../app/model/predicates.ts";

const pro = BOARD_CAPS.pro, std = BOARD_CAPS.standard;

test("labels and nodes by id", () => {
  assert.equal(labelFor("input/source"), "Source");
  assert.equal(nodeById("input/video-standard~adv.format_sv")!.setting, "adv.format_sv");
  assert.equal(nodeById("quick/source")!.setting, "input.source");
  assert.equal(nodeById("nope"), undefined);
});

test("options drop advanced-only entries in Easy but keep the current one", () => {
  const n = nodeById("input/video-standard~adv.format_sv")!;
  const easy = optionsFor(n, { mode: "easy", caps: pro, current: 0, profileActive: false }).map((o) => o.n);
  assert.deepEqual(easy, ["Auto", "Auto 7.5 IRE", "PAL", "NTSC 7.5 IRE", "PAL-60", "NTSC-J"]);
  const keep = optionsFor(n, { mode: "easy", caps: pro, current: 11, profileActive: false }).map((o) => o.n);
  assert.ok(keep.includes("SECAM"));
  assert.equal(optionsFor(n, { mode: "advanced", caps: pro, current: 0, profileActive: false }).length, 13);
});

test("options follow the board caps and Custom needs a profile", () => {
  const n = nodeById("input/source")!;
  assert.ok(optionsFor(n, { mode: "advanced", caps: pro, current: 1, profileActive: false }).some((o) => o.n === "S-Video"));
  assert.ok(!optionsFor(n, { mode: "advanced", caps: std, current: 1, profileActive: false }).some((o) => o.n === "S-Video"));
  const r = { id: "x", type: "setting", label: "Resolution", help: "", flags: [], pred: "none", setting: "output.preset" };
  assert.ok(!optionsFor(r, { mode: "advanced", caps: pro, current: 5, profileActive: false }).some((o) => o.n === "Custom"));
  assert.ok(optionsFor(r, { mode: "advanced", caps: pro, current: 5, profileActive: true }).some((o) => o.n === "Custom"));
});

test("filter choice reads and writes both settings", () => {
  assert.equal(choiceValue("filter", { "picture.sharpness": 31, "picture.line_filter": 1 }), 1);
  assert.equal(choiceValue("filter", { "picture.sharpness": 31, "picture.line_filter": 0 }), -1);   // Custom: line filter off
  assert.deepEqual(choiceWrites("filter", 2), [["picture.sharpness", 0], ["picture.line_filter", 1]]);
});

test("resolution choice: aliases, advanced and original options", () => {
  const i = choiceValue("resolution", { "output.preset": 4, "output.match_preset": 1 });
  assert.equal(choiceOptions("resolution", { mode: "easy", caps: pro })[choiceOptions("resolution", { mode: "easy", caps: pro }).findIndex((o) => o.index === i)].label, "960p/1024p");
  const easy = choiceOptions("resolution", { mode: "easy", caps: pro }).map((o) => o.label);
  assert.ok(!easy.includes("960p fixed") && !easy.includes("Downscale"));
  assert.ok(choiceOptions("resolution", { mode: "advanced", caps: std }).some((o) => o.label === "Downscale"));
  assert.equal(new Set(easy).size, easy.length);   // an alias is never offered
  assert.equal(choiceValue("resolution", { "output.preset": 2, "output.match_preset": 1 }, { basePreset: 0 }),
               choiceValue("resolution", { "output.preset": 0, "output.match_preset": 1 }));   // Custom reads the base
});

test("scanlines Easy from the choice table, Custom otherwise", () => {
  assert.deepEqual(scanlineOptions("easy").map((o) => o.label), ["Off", "Light", "Medium", "Strong"]);
  assert.equal(scanlineValue("easy", { "picture.scanlines": 1, "picture.scanline_strength": 48 }), "Medium");
  assert.equal(scanlineValue("easy", { "picture.scanlines": 1, "picture.scanline_strength": 32 }), "Custom");
  assert.deepEqual(scanlineOptions("easy", { "picture.scanlines": 1, "picture.scanline_strength": 32 }).map((o) => o.label),
                   ["Off", "Light", "Medium", "Strong", "Custom"]);
  assert.deepEqual(scanlineWrites("easy", "Light"), [["picture.scanlines", 1], ["picture.scanline_strength", 80]]);   // lower offset = darker: Light is the highest
});

test("scanlines Advanced from the range", () => {
  assert.deepEqual(scanlineOptions("advanced").map((o) => o.label), ["Off", "10", "20", "30", "40", "50", "60"]);
  assert.equal(scanlineValue("advanced", { "picture.scanlines": 1, "picture.scanline_strength": 0x40 }), "40");
  assert.deepEqual(scanlineWrites("advanced", "20"), [["picture.scanlines", 1], ["picture.scanline_strength", 32]]);
});

test("scanlines Off ignores the strength", () => {
  assert.equal(scanlineValue("easy", { "picture.scanlines": 0, "picture.scanline_strength": 80 }), "Off");
  assert.equal(scanlineValue("advanced", { "picture.scanlines": 0, "picture.scanline_strength": 80 }), "Off");
  assert.deepEqual(scanlineWrites("easy", "Off"), [["picture.scanlines", 0]]);   // sid1_only: the strength stays
});

const g = (name: string, rows: number, col?: 0 | 1): { name: string; rows: number[]; col?: 0 | 1; title?: string; cont?: boolean } =>
  ({ name, rows: new Array(rows).fill(0), col });

test("columns keep groups whole and order", () => {
  const r = balance([g("a", 5), g("b", 2), g("c", 4), g("d", 1), g("e", 3)]);
  const names = (l: { name: string }[]) => l.map((x) => x.name);
  // heaviest first into the lighter column: a(5) L, c(4) R, e(3) R=7, b(2) L=7, d(1) L on a tie; then original order
  assert.deepEqual(names(r.cols[0]), ["a", "b", "d"]);
  assert.deepEqual(names(r.cols[1]), ["c", "e"]);
  const all = [...r.cols[0], ...r.cols[1]].map((x) => x.name).sort();
  assert.deepEqual(all, ["a", "b", "c", "d", "e"]);
  const f = balance([g("x", 1), g("y", 9, 0), g("z", 1)]);   // a fixed group stays in its column
  assert.deepEqual(names(f.cols[0]), ["y"]);
  assert.deepEqual(names(f.cols[1]), ["x", "z"]);
});

// A section with one group shows two separate cards, its rows split in half (the left one gets the odd row); a
// one-row group stays in the left column
test("a single group splits its rows into two cards, the left one first", () => {
  const r = balance([g("only", 5)]);
  assert.deepEqual(r.cols[0].map((x) => x.rows.length), [3]);
  assert.deepEqual(r.cols[1].map((x) => x.rows.length), [2]);
  assert.equal(r.cols[0][0].name, "only");
  assert.equal(r.cols[1][0].title, undefined);                  // the right card continues the group, no title
  assert.equal(r.cols[1][0].cont, true);
  assert.deepEqual([...r.cols[0][0].rows, ...r.cols[1][0].rows], g("only", 5).rows);
  const one = balance([g("one", 1)]);
  assert.deepEqual(one.cols[0].map((x) => x.rows.length), [1]);
  assert.deepEqual(one.cols[1], []);
});

test("version order", () => {
  const order = ["3.0.0-x", "3.0.0-dev2", "3.0.0-dev10", "3.0.0-beta1", "3.0.0", "3.0.1-dev0", "3.1.0", "10.0.0"];
  for (let i = 0; i < order.length; i++)
    for (let j = 0; j < order.length; j++)
      assert.equal(compareVersions(order[i], order[j]), Math.sign(i - j), `${order[i]} vs ${order[j]}`);
});

test("image version read from a gzip", async () => {
  const img = Buffer.concat([Buffer.alloc(70000, 0xaa), Buffer.from("GBSCVER=3.0.2-beta1;"), Buffer.alloc(100, 0)]);
  assert.equal(await readImageVersion(new Blob([gzipSync(img)])), "3.0.2-beta1");
  assert.equal(await readImageVersion(new Blob([img])), "3.0.2-beta1");             // a raw image too
  assert.equal(await readImageVersion(new Blob([Buffer.alloc(5000, 1)])), null);
  const trailer = Buffer.concat([gzipSync(img), Buffer.alloc(300, 7)]);               // an old signed image
  assert.equal(await readImageVersion(new Blob([trailer])), "3.0.2-beta1");
});

test("extra hosts rule: the firmware's check, said in words", async () => {
  const { hostsProblem } = await import("../../app/model/hosts.ts");
  const { extraHostsValid } = await import("../../sim/guard.ts");
  const lists = ["", "gbsc.local", "a.lan, b.lan", " tv-box ,cab.fritz.box", "a b", "a,,b", "a,", "192.168.1.9", "localhost", "LOCALHOST",
    "under_score", "x".repeat(64), "a.lan,b.lan,c.lan,d.lan,e.lan,f.lan,g.lan,h.lan,i.lan,j.lan,k.lan", "ok.lan,1.2.3.4"];
  for (const l of lists) assert.equal(hostsProblem(l) === null, extraHostsValid(l), l);   // the same verdict as the device
  assert.equal(hostsProblem("gbsc.local, tv.lan"), null);
  assert.match(hostsProblem("under_score")!, /letters, digits, dots and dashes/);
  assert.match(hostsProblem("a,,b")!, /empty/);
  assert.match(hostsProblem("192.168.1.9")!, /address/);
  assert.match(hostsProblem("localhost")!, /localhost/);
  assert.match(hostsProblem("x".repeat(64))!, /63/);
  assert.match(hostsProblem("a b")!, /space/);
});

test("the recommended offer: a newer version only, never none, an error or what already runs", async () => {
  const { offerVersion } = await import("../../app/model/version.ts");
  assert.equal(offerVersion("3.0.1", "", "3.0.0"), "3.0.1");
  assert.equal(offerVersion("", "3.0.1", "3.0.0"), "3.0.1");            // s:status.update_available before the next /api/info
  assert.equal(offerVersion("", "none", "3.0.0"), "");
  assert.equal(offerVersion("", "error:no_wifi", "3.0.0"), "");
  assert.equal(offerVersion("", "3.0.1", "3.0.1"), "");                  // installed: the old status line stays in the store
  assert.equal(offerVersion("3.0.1", "", ""), "3.0.1");                  // no /api/info yet
});

// Lower strength = darker lines (Strong 0x10, Light 0x50): the card thumbnails follow it
test("scanline thumbnails: lower strength is darker", () => {
  assert.ok(scanlineDarkness(0x10) > scanlineDarkness(0x30));
  assert.ok(scanlineDarkness(0x30) > scanlineDarkness(0x50));
  assert.equal(scanlineDarkness(0x50), 10);   // the Light card's level
  assert.equal(scanlineDarkness(0x10), 50);   // the Strong card's level
});

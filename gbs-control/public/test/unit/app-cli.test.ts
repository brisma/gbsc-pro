// The ADV console on the page: the ADV Controller CLI syntax
import { test } from "node:test";
import assert from "node:assert/strict";
import { parseCli } from "../../app/regs/cli.ts";

test("cli parses every command and refuses the rest", () => {
  assert.deepEqual(parseCli("w 42 0A 10"), { kind: "write", change: { i2c: 0x42, map: 0, reg: 0x0a, value: 0x10 } });
  assert.deepEqual(parseCli("WM 42 40 80 80"), { kind: "write", change: { i2c: 0x42, map: 0x40, reg: 0x80, value: 0x80 } });
  assert.deepEqual(parseCli("r 56 17"), { kind: "read", dev: 0x56, map: 0, reg: 0x17 });
  assert.deepEqual(parseCli("rm 42 20 10"), { kind: "read", dev: 0x42, map: 0x20, reg: 0x10 });
  assert.deepEqual(parseCli("d 84"), { kind: "dump", dev: 0x84, map: 0, reg: 0, count: 256 });
  assert.deepEqual(parseCli("d 42 10 20"), { kind: "dump", dev: 0x42, map: 0, reg: 0x10, count: 0x20 });
  assert.deepEqual(parseCli("dm 42 40 80 10"), { kind: "dump", dev: 0x42, map: 0x40, reg: 0x80, count: 0x10 });
  assert.deepEqual(parseCli("dm 42 40"), { kind: "dump", dev: 0x42, map: 0x40, reg: 0, count: 256 });
  assert.deepEqual(parseCli("  info "), { kind: "info" });
  assert.deepEqual(parseCli("help"), { kind: "help" });
  assert.deepEqual(parseCli("log"), { kind: "usb", name: "log" });
  assert.deepEqual(parseCli("update"), { kind: "usb", name: "update" });
  const bad = ["", "x", "w 42 0A", "w 43 0A 10", "w 42 0A 100", "wm 56 40 01 00", "wm 42 60 01 00", "wm 84 20 41 00",
    "r 42", "d 42 F0 20", "d 42 10 0", "rm 42 40", "w 42 0A 10 11", "w 42 GG 10"];
  for (const c of bad) assert.equal(parseCli(c).kind, "error", c);
  assert.match((parseCli("w 43 0A 10") as { message: string }).message, /address/);
  assert.match((parseCli("d 42 F0 20") as { message: string }).message, /0xFF/);
});

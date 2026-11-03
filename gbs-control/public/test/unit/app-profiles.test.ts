// Profiles: operations that wait for status.profile_op, and the zip writer/reader for import and export
import { test } from "node:test";
import assert from "node:assert/strict";
import { deflateRawSync } from "node:zlib";
import { execFileSync } from "node:child_process";
import { mkdtempSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { signal } from "@preact/signals";
import { runOp, profileLabel, opErrorText } from "../../app/store/profiles.ts";
import { zipWrite, zipRead, crc32 } from "../../app/model/zip.ts";
import { json } from "./app-helpers.ts";
import { python } from "../../tools/python.mjs";

const enc = (s: string) => new TextEncoder().encode(s);
const dec = (b: Uint8Array) => new TextDecoder().decode(b);

function deps(answer: Response) {
  const profileOp = signal({ seq: 0, result: "" });
  const calls: string[] = [];
  let reloads = 0;
  return {
    profileOp, calls, reloads: () => reloads,
    d: {
      post: async (path: string, form?: Record<string, string> | FormData) => { calls.push(`${path} ${form instanceof FormData ? "form" : new URLSearchParams(form as Record<string, string>)}`); return answer; },
      profileOp, reload: async () => { reloads++; }, timeoutMs: 200,
    },
  };
}

test("runOp resolves on the WS result and times out", async () => {
  const a = deps(json(202, { ok: 1 }));
  const p = runOp(a.d, "/api/profiles/saveas", { name: "Saturn", region: "PAL" });
  await new Promise((r) => setTimeout(r, 10));
  a.profileOp.value = { seq: 1, result: "ok" };
  assert.equal(await p, "ok");
  assert.deepEqual(a.calls, ["/api/profiles/saveas name=Saturn&region=PAL"]);
  assert.equal(a.reloads(), 1);
  const b = deps(json(202, { ok: 1 }));
  const q = runOp(b.d, "/api/profiles/duplicate", { id: "p01" });
  b.profileOp.value = { seq: 1, result: "error:limit" };
  assert.equal(await q, "limit");
  const c = deps(json(202, { ok: 1 }));
  assert.equal(await runOp(c.d, "/api/profiles/delete", { id: "p01" }), "timeout");   // no result in 200 ms
  assert.equal(c.reloads(), 1);                                                         // the list shows the truth anyway
});

test("runOp reports refusals without waiting", async () => {
  assert.equal(await runOp(deps(json(409, { error: "busy" })).d, "/api/profiles/load", { id: "p01" }), "busy");
  assert.equal(await runOp(deps(json(404, { error: "unknown" })).d, "/api/profiles/load", { id: "p09" }), "unknown");
  assert.equal(await runOp(deps(json(400, { ok: 0, errors: { region: "invalid" } })).d, "/api/profiles/saveas", { name: "x", region: "Mars" }), "region invalid");
});

test("labels and error texts", () => {
  assert.equal(profileLabel({ name: "Mega Drive", region: "PAL" }), "Mega Drive · PAL");
  assert.equal(profileLabel({ name: "Saturn", region: "None" }), "Saturn");
  assert.match(opErrorText("limit"), /36 of 36/);
  assert.match(opErrorText("storage_full"), /storage/i);
});

test("zip writer output reads back with the zip reader and with Python's zipfile", async () => {
  const files = [{ name: "Mega Drive.txt", data: enc("schema=1\nprofile.name=Mega Drive\n") }, { name: "SNES.txt", data: enc("schema=1\n") }];
  const z = zipWrite(files);
  const back = await zipRead(z);
  assert.deepEqual(back.map((f) => [f.name, dec(f.data)]), files.map((f) => [f.name, dec(f.data)]));
  const dir = mkdtempSync(join(tmpdir(), "zip-"));
  writeFileSync(join(dir, "p.zip"), z);
  const list = execFileSync(python(), ["-m", "zipfile", "-l", join(dir, "p.zip")], { encoding: "utf8" });
  assert.match(list, /Mega Drive\.txt/);
  assert.match(list, /SNES\.txt/);
  assert.equal(crc32(enc("123456789")), 0xcbf43926);
});

// A zip made elsewhere: deflated entries (inflated with DecompressionStream("deflate-raw")) and stored ones
function zipOf(entries: { name: string; data: Uint8Array; deflate: boolean }[]): Uint8Array {
  const parts: number[] = [], central: number[] = [];
  const u16 = (a: number[], v: number) => a.push(v & 255, (v >> 8) & 255);
  const u32 = (a: number[], v: number) => a.push(v & 255, (v >> 8) & 255, (v >> 16) & 255, (v >>> 24) & 255);
  for (const e of entries) {
    const body = e.deflate ? new Uint8Array(deflateRawSync(e.data)) : e.data;
    const name = enc(e.name), off = parts.length, crc = crc32(e.data);
    u32(parts, 0x04034b50); u16(parts, 20); u16(parts, 0); u16(parts, e.deflate ? 8 : 0); u16(parts, 0); u16(parts, 0);
    u32(parts, crc); u32(parts, body.length); u32(parts, e.data.length); u16(parts, name.length); u16(parts, 0);
    parts.push(...name, ...body);
    u32(central, 0x02014b50); u16(central, 20); u16(central, 20); u16(central, 0); u16(central, e.deflate ? 8 : 0); u16(central, 0); u16(central, 0);
    u32(central, crc); u32(central, body.length); u32(central, e.data.length); u16(central, name.length); u16(central, 0); u16(central, 0);
    u16(central, 0); u16(central, 0); u32(central, 0); u32(central, off); central.push(...name);
  }
  const eocd: number[] = [];
  u32(eocd, 0x06054b50); u16(eocd, 0); u16(eocd, 0); u16(eocd, entries.length); u16(eocd, entries.length);
  u32(eocd, central.length); u32(eocd, parts.length); u16(eocd, 0);
  return new Uint8Array([...parts, ...central, ...eocd]);
}

test("zip reader handles stored and deflated entries", async () => {
  const text = "schema=1\n" + "picture.brightness=50\n".repeat(40);
  const z = zipOf([{ name: "a.txt", data: enc(text), deflate: true }, { name: "dir/b.txt", data: enc("x"), deflate: false }]);
  const back = await zipRead(z);
  assert.deepEqual(back.map((f) => [f.name, dec(f.data)]), [["a.txt", text], ["dir/b.txt", "x"]]);
  await assert.rejects(zipRead(new Uint8Array([1, 2, 3])), /not a zip/);
});

// Developer reads: /api/dev/regs (dev.tv5725_read), /api/dev/snapshot (dev.screen_snapshot), /api/adv/read (dev.adv_read),
// /api/regmap (the register map file, gen/regmap.json gzipped as the firmware embeds it)
import { readFileSync } from "node:fs";
import { gzipSync } from "node:zlib";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import type { Ctx } from "./http.ts";
import { sendJson } from "./http.ts";
import type { Sim } from "./server.ts";

export const tvReg = (seg: number, reg: number) => (seg * 37 + reg * 13) & 0xff;      // seeded register file (state.tvTable)
export const advReg = (dev: number, map: number, reg: number) => (dev ^ map ^ (reg * 7)) & 0xff;   // seeded (state.advValue)

export function devRegsGet(sim: Sim, ctx: Ctx): void {
  if (!sim.state.devmode) return sendJson(ctx, 404, { error: "none" });
  const r = sim.state.tvRegs;
  const hex = r.state === "done" ? Array.from(sim.state.tvTable[r.seg], (v) => v.toString(16).padStart(2, "0")).join("") : "";
  sendJson(ctx, 200, { state: r.state, seg: r.seg, gen: r.gen, hex });
}

export function devSnapshotGet(sim: Sim, ctx: Ctx): void {
  if (!sim.state.devmode || !sim.state.snapshot) return sendJson(ctx, 404, { error: "none" });
  ctx.res.writeHead(200, { "Content-Type": "application/octet-stream", "Cache-Control": "no-store" });
  ctx.res.end(sim.state.snapshot);
}

export function advReadGet(sim: Sim, ctx: Ctx): void {
  if (!sim.state.devmode || sim.state.board !== "pro") return sendJson(ctx, 404, { error: "none" });
  const r = sim.state.advRead;
  if (r.announced !== r.gen) return sendJson(ctx, 200, { state: "pending", gen: r.announced });
  const values = r.state === "done" ? Array.from({ length: r.count }, (_, i) => sim.state.advValue(r.dev, r.map, r.reg + i)) : [];
  sendJson(ctx, 200, { state: r.state, dev: r.dev, map: r.map, reg: r.reg, values, gen: r.gen });
}

let regmapGz: Buffer | null = null;
export function regmapGet(sim: Sim, ctx: Ctx): void {
  if (sim.faults.has("noRegmap")) return sendJson(ctx, 404, { error: "none" });   // built with GBSC_NO_REGMAP=1
  regmapGz ??= gzipSync(readFileSync(join(dirname(fileURLToPath(import.meta.url)), "..", "gen", "regmap.json")), { level: 9 });
  ctx.res.writeHead(200, { "Content-Type": "application/json", "Content-Encoding": "gzip", "Cache-Control": "no-cache" });
  ctx.res.end(regmapGz);
}

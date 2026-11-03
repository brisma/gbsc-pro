// Test-only endpoints (never in gen/routes.json): scenario, faults, network profile, TV-side changes, request log
import type { Ctx } from "./http.ts";
import { sendJson } from "./http.ts";
import type { Sim } from "./server.ts";
import { applyScenario } from "./scenarios.ts";
import { parseSetting } from "./validate.ts";
import { LIVE_DEFAULTS } from "./state.ts";

export const FAULTS = ["busy503", "busy503All", "busyOnce", "queueFull", "wsDrop", "wsRefuse", "slowUpload", "uploadFail", "restoreFail", "hostsInUse", "uploadHold", "wsSilent", "uploadBadRequest", "noRegmap", "advSpace", "bigBackup", "regsBusy", "advNoReply", "vppNoReply", "advUpdateHold", "updateNotApplied"];

function body(ctx: Ctx): any { try { return JSON.parse(ctx.body.toString("utf8") || "{}"); } catch { return {}; } }

export function scenarioPost(sim: Sim, ctx: Ctx): void {
  const b = body(ctx);
  if (!applyScenario(sim, String(b.name))) return sendJson(ctx, 400, { error: "unknown scenario" });
  sendJson(ctx, 200, { ok: 1 });
}

export function faultPost(sim: Sim, ctx: Ctx): void {
  const b = body(ctx);
  if (!FAULTS.includes(b.name)) return sendJson(ctx, 400, { error: "unknown fault" });
  if (b.on === false) sim.faults.delete(b.name); else sim.faults.add(b.name);
  if (b.name === "wsDrop" && b.on !== false) { sim.ws.dropAll(); sim.faults.delete("wsDrop"); }
  sim.ws.refuse = sim.faults.has("wsRefuse");
  sim.ws.silent = sim.faults.has("wsSilent");
  sendJson(ctx, 200, { ok: 1 });
}

export function netPost(sim: Sim, ctx: Ctx): void {
  const p = body(ctx).profile;
  if (p !== "fast" && p !== "device" && p !== "slow") return sendJson(ctx, 400, { error: "unknown profile" });
  sim.net = p;
  sendJson(ctx, 200, { ok: 1 });
}

// A change made on the TV or OLED: it goes through the same queue as a web write (bridged rules included)
export function tvPost(sim: Sim, ctx: Ctx): void {
  const b = body(ctx);
  const e = sim.state.entry(String(b.key));
  if (!e || e.type === "str") return sendJson(ctx, 400, { error: "unknown key" });
  const v = parseSetting(e, String(b.value));
  if (v === null) return sendJson(ctx, 400, { error: "invalid value" });
  sendJson(ctx, 200, { ok: 1 });
  sim.later(sim.tvLatencyMs, () => sim.state.applyNumber(e.key, v));
}

// A readback the device would report (/api/live): {name, value} with value a number or [h, v]
export function livePost(sim: Sim, ctx: Ctx): void {
  const b = body(ctx);
  if (!(String(b.name) in LIVE_DEFAULTS)) return sendJson(ctx, 400, { error: "unknown live value" });
  const pair = Array.isArray(LIVE_DEFAULTS[b.name]);
  const ok = pair ? Array.isArray(b.value) && b.value.length === 2 && b.value.every(Number.isFinite) : Number.isFinite(b.value);
  if (!ok) return sendJson(ctx, 400, { error: "invalid value" });
  sim.state.live[b.name] = b.value;
  sendJson(ctx, 200, { ok: 1 });
}

// A console line (log ring + WebSocket l:) and a history event (event ring + e:), as the device prints them
const LOG_CATS = ["gbs", "sys", "input", "scan", "sync", "preset", "profile", "adv", "link", "api", "wifi", "update", "menu"];
export function logPost(sim: Sim, ctx: Ctx): void {
  const b = body(ctx);
  if (!LOG_CATS.includes(b.cat)) return sendJson(ctx, 400, { error: "unknown category" });
  sim.state.log(b.cat, b.level === "debug" ? "debug" : "info", String(b.msg ?? ""));
  sendJson(ctx, 200, { ok: 1 });
}
export function eventPost(sim: Sim, ctx: Ctx): void {
  const b = body(ctx);
  sim.state.event(String(b.type), Number(b.a ?? 0), Number(b.b ?? 0));
  sendJson(ctx, 200, { ok: 1 });
}

export function requestsGet(sim: Sim, ctx: Ctx): void {
  sendJson(ctx, 200, { requests: sim.requestLog, maxInFlight: sim.maxInFlight, duringUpload: sim.duringUpload });
}

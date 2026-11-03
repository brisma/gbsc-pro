// Safe mode (API_ROUTES_SAFE): a minimal page and the restart action
import type { Ctx } from "./http.ts";
import { sendJson, sendGzip } from "./http.ts";
import type { Sim } from "./server.ts";

export function safePageGet(_sim: Sim, ctx: Ctx): void {
  sendGzip(ctx, 200, "text/html", "<!doctype html><title>GBSC PRO safe mode</title><h1>Safe mode</h1><p>Upload a firmware or restart.</p>");
}

export function safeRestartPost(sim: Sim, ctx: Ctx): void {
  sendJson(ctx, 200, { ok: 1 });
  sim.restart();
}

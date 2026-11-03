// /, /api/info, /api/settings, /api/schema, /api/actions, /api/log, /api/events
import { readFileSync } from "node:fs";
import { join, dirname } from "node:path";
import type { Ctx } from "./http.ts";
import { arg, sendJson, sendGzip } from "./http.ts";
import type { Sim } from "./server.ts";
import { validateRequest } from "./validate.ts";
import { validateAction } from "./actions-validate.ts";
import { hostLostByChange } from "./guard.ts";

export function pageGet(sim: Sim, ctx: Ctx): void { servePage(sim, ctx, sim.pagePath); }

// GET /__sim/components: the kitchen sink of the row components (dist/components.html beside the app page)
export function componentsGet(sim: Sim, ctx: Ctx): void { servePage(sim, ctx, join(dirname(sim.pagePath), "components.html")); }

function servePage(sim: Sim, ctx: Ctx, path: string): void {
  const html = readFileSync(path, "utf8");
  const inject = `<script>window.GBSC_WS_URL="ws://127.0.0.1:${sim.wsPort}/"</script>`;
  const out = html.includes("<head>") ? html.replace("<head>", "<head>" + inject) : inject + html;
  sendGzip(ctx, 200, "text/html", out);
}

export function infoGet(sim: Sim, ctx: Ctx): void { sendJson(ctx, 200, { ...sim.state.info(), safe_mode: sim.safeMode }); }
export function settingsGet(sim: Sim, ctx: Ctx): void { sendJson(ctx, 200, sim.state.settingsJson()); }

export function schemaGet(sim: Sim, ctx: Ctx): void {
  sendJson(ctx, 200, sim.gen.schema[sim.state.board + (sim.state.devmode ? ".dev" : "")]);
}

export function actionsListGet(sim: Sim, ctx: Ctx): void {
  sendJson(ctx, 200, sim.gen.actions[sim.state.board + (sim.state.devmode ? ".dev" : "")]);
}

// settingsPost (pro/settings/settings-web.cpp)
export function settingsPost(sim: Sim, ctx: Ctx): void {
  const st = sim.state;
  if (ctx.form.length > 24) return sendJson(ctx, 400, { error: "too_many" });
  if (sim.faults.has("queueFull")) return sendJson(ctx, 409, { error: "busy" });
  const r = validateRequest(sim.gen.registry, st.caps, st.devmode, ctx.form.map(([k]) => k), ctx.form.map(([, v]) => v));
  if (r.str && r.str.key === "system.extra_hosts" && (sim.faults.has("hostsInUse") ||
      hostLostByChange(ctx.req.headers.host, String(st.values.get("system.extra_hosts") ?? ""), r.str.value))) {
    st.log("api", "info", `extra hosts: refused, drops the name of this page (${ctx.req.headers.host})`);
    return sendJson(ctx, 409, { error: "in_use" });
  }
  sendJson(ctx, r.errCount ? 400 : 200, { ok: r.ok, errors: r.errors });
  sim.pendingApplies++;
  sim.later(sim.applyDelayMs, () => {   // applied by the loop after the answer, as apiLoop()
    if (r.str) st.applyString(r.str.key, r.str.value);
    for (const p of r.pairs) st.applyNumber(p.key, p.value);
    sim.pendingApplies--;
  });
}

// actionsPost (pro/api/api-web.cpp)
export function actionsPost(sim: Sim, ctx: Ctx): void {
  const st = sim.state;
  const name = ctx.url.pathname.slice("/api/actions/".length);
  const c = validateAction(sim.gen.actionsAll, st.caps, st.devmode, name, arg(ctx, "data"));
  if (c.status === 404) return sendJson(ctx, 404, { error: "unknown" });
  if (c.status === 400) return sendJson(ctx, 400, { ok: 0, errors: { data: "invalid" } });
  if (name === "input.termination" && st.runtime.activeInput !== 1) {
    st.log("api", "info", `termination: refused, input ${st.runtime.activeInput}`);
    return sendJson(ctx, 409, { error: "not_rgb_scart" });
  }
  if (sim.faults.has("queueFull")) return sendJson(ctx, 409, { error: "busy" });
  const result = sim.runAction(name, c.parsed, c.text);
  sendJson(ctx, 200, result ?? { ok: 1 });
}

export function logGet(sim: Sim, ctx: Ctx): void {
  const since = Number(arg(ctx, "since") ?? -1), boot = Number(arg(ctx, "boot") ?? -1);
  sendJson(ctx, 200, sim.state.logJson(Number.isFinite(since) && since <= 65535 ? since : -1, Number.isFinite(boot) ? boot : -1));
}

// GET /api/live (api-web.cpp liveGet): the readback cache; values not read yet are absent (here: always read)
export function liveGet(sim: Sim, ctx: Ctx): void {
  sendJson(ctx, 200, { at: Date.now() - sim.state.started, values: sim.state.live });
}

export function eventsGet(sim: Sim, ctx: Ctx): void {
  const since = Number(arg(ctx, "since") ?? 0);
  sendJson(ctx, 200, sim.state.eventsJson(Number.isFinite(since) && since <= 65535 ? since : 0));
}

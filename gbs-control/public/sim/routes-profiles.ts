// /api/profiles* (pro/profiles/profile-web.cpp): 202 when queued, the result later on the WebSocket
import type { Ctx } from "./http.ts";
import { arg, sendJson, parseMultipart } from "./http.ts";
import type { Sim } from "./server.ts";
import { indexOfId, sanitizeName, REGIONS } from "./profiles.ts";

// profile-args.cpp: region names exactly as profileRegionName; a missing region is None
const regionArg = (ctx: Ctx): string | null | false => {
  const r = arg(ctx, "region");
  if (r === null) return null;
  return REGIONS.includes(r) ? r : false;
};
const argError = (ctx: Ctx, field: "name" | "region") => sendJson(ctx, 400, { ok: 0, errors: { [field]: "invalid" } });

function queue(sim: Sim, ctx: Ctx, kind: string, i: number, name: string | null, region: string | null, text?: string): void {
  if (sim.faults.has("queueFull")) return sendJson(ctx, 409, { error: "busy" });
  sendJson(ctx, 202, { ok: 1 });
  sim.later(sim.opDelayMs, () => {
    const err = sim.profiles.run(kind, i, name, region, text);
    sim.state.send(`s:status.profile_op=${err ? "error:" + err : "ok"}`);
    sim.state.log("profile", "info", `${kind}${i >= 0 ? " p" + String(i).padStart(2, "0") : ""}: ${err ? "failed (" + err + ")" : "ok"}`);
    sim.profiles.statusTick();
  });
}

const used = (sim: Sim, ctx: Ctx) => { const i = indexOfId(arg(ctx, "id")); return i >= 0 && sim.profiles.slots[i] ? i : -1; };

export function profilesGet(sim: Sim, ctx: Ctx): void { sendJson(ctx, 200, sim.profiles.json()); }

export function profileOp(kind: string) {
  return (sim: Sim, ctx: Ctx): void => {
    const i = kind === "defaults" ? -1 : used(sim, ctx);
    if (i < 0 && kind !== "defaults") return sendJson(ctx, 404, { error: "unknown" });
    if (kind === "replace") {   // profileReplaceArgs: optional name and region, a region needs a name
      const rawName = arg(ctx, "name"), region = regionArg(ctx);
      if (rawName === null) return region === null ? queue(sim, ctx, kind, i, null, null) : argError(ctx, "region");
      const name = sanitizeName(rawName);
      if (!name) return argError(ctx, "name");
      if (region === false) return argError(ctx, "region");
      return queue(sim, ctx, kind, i, name, region ?? "None");
    }
    queue(sim, ctx, kind, i, null, null);
  };
}

export function profileSaveAs(sim: Sim, ctx: Ctx): void {
  const name = sanitizeName(arg(ctx, "name"));
  if (!name) return argError(ctx, "name");
  const region = regionArg(ctx);
  if (region === false) return argError(ctx, "region");
  queue(sim, ctx, "saveas", -1, name, region ?? "None");
}

export function profileRename(sim: Sim, ctx: Ctx): void {
  const i = used(sim, ctx);
  if (i < 0) return sendJson(ctx, 404, { error: "unknown" });
  const name = sanitizeName(arg(ctx, "name"));
  if (!name) return sendJson(ctx, 400, { ok: 0, errors: { name: "invalid" } });
  queue(sim, ctx, "rename", i, name, null);
}

export function profileExport(sim: Sim, ctx: Ctx): void {
  const i = used(sim, ctx);
  if (i < 0) return sendJson(ctx, 404, { error: "unknown" });
  const file = sim.profiles.slots[i]!.name.replace(/[/:' ]/g, "_") + ".gbscprofile.txt";   // profileExportFileName
  ctx.res.writeHead(200, { "Content-Type": "text/plain", "Content-Disposition": `attachment; filename="${file}"`, "Cache-Control": "no-store" });
  ctx.res.end(sim.profiles.exportText(i));
}

export function profileImport(sim: Sim, ctx: Ctx): void {
  const part = parseMultipart(ctx.req, ctx.body).find((p) => p.filename !== null);
  if (!part) return sendJson(ctx, 400, { ok: 0 });
  if (part.data.length > 16384) return sendJson(ctx, 413, { error: "too_big" });
  queue(sim, ctx, "import", -1, null, null, part.data.toString("utf8"));
}

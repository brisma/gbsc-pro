// /api/system/backup, restore, update (upload) and the restart that follows them
import type { Ctx } from "./http.ts";
import { sendJson, parseMultipart } from "./http.ts";
import type { Sim } from "./server.ts";
import { advBackupText, advRestore } from "./routes-adv-store.ts";

export function backupText(sim: Sim): string {
  const st = sim.state;
  const keys = (scope: string) => st.gen.registry.settings
    .filter((e) => e.scope === scope && !e.secret && !e.ro && st.values.has(e.key))
    .map((e) => `${e.key}=${st.values.get(e.key)}`).join("\n");
  const active = sim.profiles.active >= 0 ? `\nprofile.active=p${String(sim.profiles.active).padStart(2, "0")}` : "";   // read-only over HTTP, but in a backup
  let t = `# gbsc-backup 1\n[file cfg/device.cfg]\nschema=1\n${keys("device")}${active}\n[file cfg/current.cfg]\nschema=1\n${keys("profile")}\n[profiles]\n`;
  sim.profiles.slots.forEach((p, i) => { if (p) t += `[profile p${String(i).padStart(2, "0")}]\n${sim.profiles.exportText(i)}`; });
  t += advBackupText(sim);
  if (sim.faults.has("bigBackup")) t += "# padding\n".repeat(70000);   // a device full of profiles and ADV files
  return t;
}

export function backupGet(sim: Sim, ctx: Ctx): void {
  ctx.res.writeHead(200, { "Content-Type": "text/plain", "Content-Disposition": 'attachment; filename="gbsc-backup.txt"', "Cache-Control": "no-store" });
  ctx.res.end(backupText(sim));
}

export function restorePost(sim: Sim, ctx: Ctx): void {
  const part = parseMultipart(ctx.req, ctx.body).find((p) => p.filename !== null);
  if (!part) return sendJson(ctx, 400, { ok: 0 });
  if (part.data.length > 655360) return sendJson(ctx, 413, { ok: 0 });
  const text = part.data.toString("utf8");
  sendJson(ctx, 202, { ok: 1 });   // the device checks the file in the loop (runRestore) and reports on the WebSocket
  sim.later(sim.opDelayMs, () => {
    if (!text.startsWith("# gbsc-backup 1")) { sim.state.send("s:status.restore=error:header"); return; }
    if (sim.faults.has("restoreFail")) { sim.state.send("s:status.restore=error:write"); return; }
    let section = "", active: string | null = null;
    const profiles = new Map<number, string>();
    for (const line of text.split("\n")) {
      if (line.startsWith("[")) { section = line; continue; }
      const pm = /^\[profile p(\d\d)\]$/.exec(section);
      if (pm) { profiles.set(Number(pm[1]), (profiles.get(Number(pm[1])) ?? "") + line + "\n"); continue; }
      const m = /^([a-z0-9_.]+)=(.*)$/.exec(line);
      if (!m || !section.startsWith("[file ")) continue;
      if (m[1] === "profile.active") { active = m[2]; continue; }
      const e = sim.state.entry(m[1]);
      if (!e || e.ro || e.secret) continue;
      sim.state.values.set(m[1], e.type === "str" ? m[2] : Number(m[2]));
    }
    if (text.includes("\n[profiles]\n")) sim.profiles.restore(profiles, active);
    advRestore(sim, text);
    sim.state.send("s:status.restore=50");
    sim.state.send("s:status.restore=ok");
    sim.state.log("api", "info", "restore: done, restarting");
    sim.restart();
  });
}

export async function updatePost(sim: Sim, ctx: Ctx): Promise<void> {
  const st = sim.state;
  const part = parseMultipart(ctx.req, ctx.body).find((p) => p.filename !== null);
  if (!part || !part.data.length) { st.upload.state = "idle"; return sendJson(ctx, 400, { ok: 0 }); }
  st.upload = { state: "receiving", error: null, received: 0, total: part.data.length };
  st.log("update", "info", `upload started (${Math.round(part.data.length / 1024)} KB)`);
  const epoch = sim.epoch;   // a scenario reset during the upload: the next test's device is not touched
  for (let pct = 5; pct <= 100; pct += 5) {
    st.upload.received = Math.floor((part.data.length * pct) / 100);
    st.send(`s:status.update=${pct}`);
    if (sim.faults.has("slowUpload")) await new Promise((r) => setTimeout(r, 50));
    while (pct === 35 && sim.faults.has("uploadHold")) await new Promise((r) => setTimeout(r, 50));   // a still frame for the goldens
    if (sim.epoch !== epoch) return sendJson(ctx, 503, { error: "reset" });
  }
  if (sim.faults.has("uploadBadRequest")) {   // updateUploadDone answers 400 and the loop reports the reason
    st.upload = { ...st.upload, state: "failed", error: "format" };
    sendJson(ctx, 400, { ok: 0 });
    st.send("s:status.update=error:format");
    st.log("update", "info", "upload failed: format");
    return;
  }
  if (sim.faults.has("uploadFail")) {   // the body was complete: 202, then the loop's verification fails
    st.upload = { ...st.upload, state: "failed", error: "verify" };
    sendJson(ctx, 202, { ok: 1 });
    st.send("s:status.update=error:verify");
    st.log("update", "info", "upload failed: verify");
    return;
  }
  st.upload.state = "done";
  sendJson(ctx, 202, { ok: 1 });
  st.send("s:status.update=done");
  st.log("update", "info", "upload ok, restarting");
  sim.restart(sim.faults.has("updateNotApplied") ? undefined : sim.nextFw);   // fault: the device keeps its old image
}

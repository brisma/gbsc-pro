// Screenshots of the built app against the simulator, for reviews (host fonts): node tools/app-shot.mjs <outdir>
// '<json [{name, hash, w, h, mode?: "easy"|"advanced", dev?: true, scenario?, full?, click?: [selector...]}]>'
import { chromium } from "@playwright/test";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { startSim } from "../sim/server.ts";

const root = join(dirname(fileURLToPath(import.meta.url)), "..");
const [out, list] = process.argv.slice(2);
const sim = await startSim({ port: 0, wsPort: 0, pagePath: join(root, "dist", "app.html") });
const post = (path, body) => fetch(sim.url + path, { method: "POST", body: JSON.stringify(body), headers: { "content-type": "application/json" } });
const b = await chromium.launch();
for (const s of JSON.parse(list)) {
  await post("/__sim/scenario", { name: s.scenario ?? "pro-rgbs" });
  if (s.dev) {   // applied in the device's loop: wait for it, or the page lands on System
    await post("/__sim/tv", { key: "system.devmode", value: "1" });
    for (let i = 0; i < 40 && (await (await fetch(sim.url + "/api/settings")).json())["system.devmode"] !== 1; i++) await new Promise((r) => setTimeout(r, 25));
  }
  const ctx = await b.newContext({ viewport: { width: s.w, height: s.h }, deviceScaleFactor: s.w < 600 ? 2 : 1 });
  const p = await ctx.newPage();
  await p.addInitScript((m) => localStorage.setItem("gbsc.mode", JSON.stringify(m)), s.mode ?? (s.dev ? "advanced" : "easy"));
  await p.goto(sim.url + "/" + (s.hash ?? ""));
  await p.locator(".ui[data-ready]").waitFor();
  await p.waitForTimeout(700);
  for (const c of s.click ?? []) { await p.locator(c).first().click(); await p.waitForTimeout(300); }
  await p.addStyleTag({ content: "*,*::before,*::after{animation:none!important;transition:none!important}" + (s.full ? " .ui{height:auto!important;overflow:visible!important} .ui .list{overflow:visible!important}" : "") });
  await p.screenshot({ path: join(out, s.name + ".png"), fullPage: !!s.full });
  await ctx.close();
}
await b.close();
await sim.stop();

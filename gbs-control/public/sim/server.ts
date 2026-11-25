// Device simulator: every route of API_ROUTES_NORMAL / API_ROUTES_SAFE, the WebSocket and the guard, with the data the
// firmware exports (public/gen/, npm run gen)
import http from "node:http";
import type { AddressInfo } from "node:net";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import type { Gen, SimOptions } from "./types.ts";
import { loadGen } from "./load.ts";
import { DeviceState } from "./state.ts";
import { compareVersions } from "../app/model/version.ts";   // the device's frozen version order (update-manifest.cpp)
import { WsHub } from "./ws.ts";
import { hostAllowed, originAllowed } from "./guard.ts";
import type { Ctx } from "./http.ts";
import { sendJson, parseForm } from "./http.ts";
import * as core from "./routes-core.ts";
import * as prof from "./routes-profiles.ts";
import * as sys from "./routes-system.ts";
import * as wifi from "./routes-wifi.ts";
import * as dev from "./routes-dev.ts";
import * as advStore from "./routes-adv-store.ts";
import * as safe from "./safe.ts";
import { ProfileStore } from "./profiles.ts";
import * as simapi from "./simapi.ts";
import { testPageGet } from "./testpages.ts";
import { Admission, exempt } from "./admission.ts";
import { PROFILES, sleep, type NetProfile } from "./net.ts";
import type { Board } from "./types.ts";

export type Handler = (sim: Sim, ctx: Ctx) => void | Promise<void>;
interface Route { method: string; path: string; prefix: boolean; safe: boolean; normal: boolean; handler: Handler }

const here = dirname(fileURLToPath(import.meta.url));

export class Sim {
  gen: Gen;
  state: DeviceState;
  ws: WsHub;
  http: http.Server;
  pagePath: string;
  url = "";
  wsUrl = "";
  wsPort = 0;
  faults = new Set<string>();
  profiles: ProfileStore;
  nextFw: string;
  opDelayMs = 50;
  advUpdateMs = 600;      // an ADV Controller image copy (seconds on the device)
  advRestartMs = 400;     // the link restart after it: PING -> INFO with the new firmware
  restartMs = 300;
  restarting = false;
  epoch = 0;   // +1 on every scenario reset: timers of the previous test do nothing
  applyDelayMs = 5;
  tvLatencyMs = 100;
  net: NetProfile = "fast";
  admission = new Admission();
  admittedInFlight = 0;
  inFlight = 0;
  uploadInFlight = 0;
  maxInFlight = 0;
  duringUpload = 0;
  requestLog: { method: string; path: string; body: string; t0: number; t1: number }[] = [];
  pendingApplies = 0;
  safeMode = false;
  routes: Route[] = [];
  adv = new advStore.AdvStore();   // LittleFS files: kept across restarts, cleared by a new scenario
  private timers: NodeJS.Timeout[] = [];
  private gens = { tv5725: 0, adv: 0 };

  constructor(opts: SimOptions) {
    this.gen = loadGen(opts.genDir);
    this.state = new DeviceState(this.gen, opts.board ?? "pro");
    this.pagePath = opts.pagePath ?? join(here, "..", "dist", "app.html");
    this.ws = new WsHub(() => String(this.state.values.get("system.extra_hosts") ?? ""), () => this.safeMode);
    this.profiles = new ProfileStore(this.state);
    this.wireState();
    this.nextFw = opts.nextFw ?? "3.0.1-dev0";
    this.net = opts.net ?? "fast";
    this.http = http.createServer((req, res) => { this.handle(req, res).catch((e) => { res.writeHead(500); res.end(String(e)); }); });
    this.addCoreRoutes();
  }

  private wireState(): void {
    this.state.send = (m) => this.ws.broadcast(m);
    this.state.onSettingChange = (k) => this.profiles.settingChanged(k);
    this.state.netLow = () => this.admission.low;
  }

  // A fresh device (POST /__sim/scenario): new state and profiles, no faults, fast network, empty request log
  reset(board: Board): void {
    this.epoch++;
    this.restarting = false;
    this.pendingApplies = 0;
    this.state = new DeviceState(this.gen, board);
    this.profiles = new ProfileStore(this.state);
    this.adv = new advStore.AdvStore();
    this.wireState();
    this.faults.clear();
    this.ws.refuse = false;
    this.ws.silent = false;
    this.net = "fast";
    this.safeMode = false;
    this.admission = new Admission();
    this.requestLog = [];
    this.maxInFlight = 0;
    this.duringUpload = 0;
  }

  // setTimeout that does nothing once the scenario was reset (a restart or op of an earlier test)
  later(ms: number, fn: () => void): void {
    const e = this.epoch;
    setTimeout(() => { if (e === this.epoch) fn(); }, ms);
  }

  route(method: string, path: string, handler: Handler, where: { safe?: boolean; normal?: boolean; prefix?: boolean } = {}): void {
    this.routes.push({ method, path, handler, prefix: !!where.prefix, safe: !!where.safe, normal: where.normal !== false });
  }

  private addCoreRoutes(): void {
    this.route("GET", "/", core.pageGet);
    this.route("GET", "/", safe.safePageGet, { safe: true, normal: false });
    this.route("GET", "/api/info", core.infoGet, { safe: true });
    this.route("GET", "/api/log", core.logGet, { safe: true });
    this.route("GET", "/api/events", core.eventsGet);
    this.route("GET", "/api/live", core.liveGet);
    this.route("GET", "/api/settings", core.settingsGet);
    this.route("POST", "/api/settings", core.settingsPost);
    this.route("GET", "/api/schema", core.schemaGet);
    this.route("GET", "/api/actions", core.actionsListGet);
    this.route("POST", "/api/actions/", core.actionsPost, { prefix: true });
    this.route("POST", "/api/actions/system.restart", safe.safeRestartPost, { safe: true, normal: false });
    this.route("GET", "/api/dev/snapshot", dev.devSnapshotGet);
    this.route("GET", "/api/dev/regs", dev.devRegsGet);
    this.route("GET", "/api/adv/read", dev.advReadGet);
    this.route("GET", "/api/regmap", dev.regmapGet);
    for (const k of ["preset", "macro"] as const) {
      this.route("GET", `/api/adv/${k}s`, advStore.listGet(k));
      this.route("GET", `/api/adv/${k}`, advStore.textGet(k));
      this.route("POST", `/api/adv/${k}`, advStore.savePost(k));
      this.route("POST", `/api/adv/${k}/delete`, advStore.deletePost(k));
    }
    this.route("GET", "/api/system/backup", sys.backupGet, { safe: true });
    this.route("POST", "/api/system/restore", sys.restorePost);
    this.route("POST", "/api/system/update", sys.updatePost, { safe: true });
    this.route("POST", "/api/system/adv-image", sys.advImagePost);
    this.route("GET", "/api/wifi", wifi.wifiGet);
    this.route("GET", "/api/wifi/scan", wifi.wifiScanGet);
    this.route("POST", "/api/wifi/connect", wifi.wifiConnectPost);
    this.route("POST", "/api/wifi/reset", wifi.wifiResetPost);
    this.route("GET", "/api/profiles", prof.profilesGet);
    for (const op of ["load", "save", "delete", "duplicate", "clear_fp", "defaults", "replace"])
      this.route("POST", "/api/profiles/" + op, prof.profileOp(op));
    this.route("POST", "/api/profiles/saveas", prof.profileSaveAs);
    this.route("POST", "/api/profiles/rename", prof.profileRename);
    this.route("GET", "/api/profiles/export", prof.profileExport);
    this.route("POST", "/api/profiles/import", prof.profileImport);
    this.route("POST", "/__sim/scenario", simapi.scenarioPost, { safe: true });
    this.route("POST", "/__sim/fault", simapi.faultPost, { safe: true });
    this.route("POST", "/__sim/net", simapi.netPost, { safe: true });
    this.route("POST", "/__sim/tv", simapi.tvPost, { safe: true });
    this.route("POST", "/__sim/live", simapi.livePost, { safe: true });
    this.route("POST", "/__sim/log", simapi.logPost, { safe: true });
    this.route("POST", "/__sim/event", simapi.eventPost, { safe: true });
    this.route("GET", "/__sim/requests", simapi.requestsGet, { safe: true });
    this.route("GET", "/__sim/test-page", testPageGet, { safe: true });
    this.route("GET", "/__sim/components", core.componentsGet, { safe: true });
  }

  // The device restarts: the WebSocket drops, connections fail for restartMs, then it comes back
  restart(fw?: string): void {
    this.later(20, () => {
      this.restarting = true;
      this.ws.refuse = true;
      this.ws.dropAll();
      this.later(this.restartMs, () => {
        this.state.reboot(fw);
        this.restarting = false;
        this.ws.refuse = this.faults.has("wsRefuse");
      });
    });
  }

  find(method: string, path: string, safe: boolean): Route | undefined {
    return this.routes.find((r) => r.method === method && (safe ? r.safe : r.normal) &&
                                   (r.prefix ? path.startsWith(r.path) : path === r.path));
  }

  // Simulated effects of an accepted action (more in the route files); the answer body when not {"ok":1}
  runAction(name: string, parsed: number[] | null, text: string | null): Record<string, unknown> | null {
    const st = this.state;
    st.log("api", name.startsWith("dev.") ? "debug" : "info", `action ${name}`);
    const flip = DEV_FLIPS[name], step = DEV_STEPS[name];   // the developer readbacks move as on the device
    if (flip) st.live[flip] = Number(st.live[flip]) ? 0 : 1;
    if (step) st.live[step[0]] = Number(st.live[step[0]]) + step[1];
    switch (name) {
      case "system.restart": this.restart(); return null;
      case "system.factory_reset": st.resetValues(); this.restart(); return null;
      case "system.check_update": this.later(this.opDelayMs, () => this.checkUpdate()); return null;
      case "system.install_update": this.installUpdate(text === "older"); return null;
      case "system.update_adv_controller":
        st.advUpdateState = "running";
        st.log("adv", "info", "update: sending the image");
        if (!this.faults.has("advUpdateHold")) this.later(this.advUpdateMs, () => {
          // as the device: "done", then the link restarts and /api/info has no version until the new INFO arrives
          st.advUpdateState = "done"; st.advUpdate = false; st.advStatus = "checking"; st.log("adv", "info", "update: done");
          st.advVersion = st.advImage.version; st.advBuild = st.advImage.build;
          if (st.advImage.source === "file") {   // installed: the device removes the file, the built-in image is offered again
            st.advImage = { source: "embedded", version: "3.0.0", build: "446FC362" };
            st.log("update", "info", `ADV Controller runs build ${st.advBuild}: image file removed`);
          }
          this.later(this.advRestartMs, () => { st.advStatus = "ok"; });
        });
        return null;
      case "dev.screen_snapshot": st.snapshot = Buffer.alloc(2048); return null;
      case "dev.adv_i2c": {   // triplets; an ADV7280 0x0E write selects the map of the next ones (reset to 0 after)
        let map = 0;
        for (let i = 0; parsed && i + 2 < parsed.length; i += 3) {
          const [dev, reg, val] = [parsed[i], parsed[i + 1], parsed[i + 2]];
          if (dev === 0x42 && reg === 0x0e) { map = val; continue; }
          st.advTable.set(`${dev}:${dev === 0x42 ? map : 0}:${reg}`, val);
        }
        return null;
      }
      case "dev.tv5725_reg":   // segment, register, value triplets
        for (let i = 0; parsed && i + 2 < parsed.length; i += 3) if (parsed[i] < 6) st.tvTable[parsed[i]][parsed[i + 1]] = parsed[i + 2];
        return null;
      case "dev.tv5725_read": {
        const gen = (this.gens.tv5725 = (this.gens.tv5725 + 1) & 0xff);
        const seg = parsed ? parsed[0] : 0;
        st.tvRegs = { state: "pending", seg, gen };
        this.later(15, () => {
          if (st.tvRegs.gen !== gen) return;   // a later request replaced it
          st.tvRegs = { state: st.upload.state === "receiving" || this.faults.has("regsBusy") ? "busy" : "done", seg, gen };
          st.log("api", "info", `tv5725 read: seg ${seg} (gen ${gen})`);
        });
        return { ok: 1, gen };
      }
      case "dev.adv_read": {
        const gen = (this.gens.adv = (this.gens.adv + 1) & 0xff);
        const [d, m, r, c] = parsed || [0x42, 0, 0, 1];
        st.advRead.announced = gen;
        this.later(15, () => {
          const ok = st.advCaps.includes("reg_read");
          const silent = st.advStatus !== "ok" || this.faults.has("advNoReply") || (d === 0x84 && this.faults.has("vppNoReply"));
          st.advRead = { state: silent ? "error" : ok ? "done" : "unsupported", dev: d, map: m, reg: r, count: c, gen, announced: st.advRead.announced };
          st.log("adv", "info", `reg read ${d.toString(16)} map ${m.toString(16)} from ${r.toString(16)} x${c}: ${ok ? "ok" : "unsupported"} (gen ${gen})`);
        });
        return { ok: 1, gen };
      }
    }
    void text;
    return null;
  }

  // ota-update.cpp finishCheck + ota-plan.cpp otaFinishCheck: the chosen channel and the more stable ones are read,
  // the newest wins and is offered whenever it differs from the running version, flagged older when it is lower
  private checkUpdate(): void {
    const st = this.state, chosen = st.updateChannel();
    let best: { version: string; channel: string } | null = null;
    for (const ch of chosen === "beta" ? ["beta", "stable"] : ["stable"]) {
      const v = st.serverBuilds[ch];
      if (v && (!best || compareVersions(v, best.version) > 0)) best = { version: v, channel: ch };
    }
    const c = best ? compareVersions(best.version, st.fw) : 0;
    const o = best && c !== 0 ? { ...best, older: c < 0 } : null;
    st.updateOffer = o;
    st.updateFor = chosen;
    st.updateLast = o ? o.version : "none";
    st.send(`s:status.update_available=${st.updateLast}`);
    st.log("update", "info", o ? `offer: ${o.version} (${o.channel}${o.older ? ", older" : ""}), channel ${chosen}` : `offer: none, channel ${chosen}`);
    if (o) st.event("update_offered", OTA_CHANNELS.indexOf(o.channel), o.older ? 1 : 0);
  }

  // otaInstall + updateLoop: download progress every 5 %, done, the restart into the new image
  private installUpdate(olderConfirmed: boolean): void {
    const st = this.state, o = st.updateFor === st.updateChannel() ? st.updateOffer : null;
    const refuse = (code: string) => { st.send(`s:status.update=error:${code}`); st.log("update", "info", `install refused: ${code}`); };
    if (!o || !!o.older !== olderConfirmed) return this.later(this.opDelayMs, () => refuse("changed"));   // otaInstallMatches
    if (st.upload.state === "receiving") return this.later(this.opDelayMs, () => refuse("busy"));
    st.log("update", "info", `install ${o.version} from ${o.channel}: started`);
    const step = (pct: number) => this.later(100, () => {   // ~2 s: the device takes about a minute
      if (pct <= 100) { st.send(`s:status.update=${pct}`); step(pct + 5); return; }
      st.send("s:status.update=done");
      st.event("update_done", 1, 0);
      st.log("update", "info", "download ok, restarting");
      this.restart(o.version);
    });
    step(5);
  }

  // As the device: admission and the request discipline count at header time; the body is read afterwards
  // (throttled in the device profile, so an upload occupies the device for as long as its body takes)
  private async handle(req: http.IncomingMessage, res: http.ServerResponse): Promise<void> {
    const url = new URL(req.url || "/", "http://sim.local");
    // the device is rebooting: connections fail (not the test endpoints, which are not the device)
    if (this.restarting && !url.pathname.startsWith("/__sim/")) { req.socket.destroy(); return; }
    const method = req.method || "GET";
    res.setHeader("Connection", "close");   // one request per connection
    if (process.env.SIM_VERBOSE) console.log(`sim: ${method} ${url.pathname}${url.search}`);
    const readBody = async (throttle: number) => {
      const chunks: Buffer[] = [];
      for await (const c of req) {
        chunks.push(c as Buffer);
        if (throttle) await sleep(Math.round(((c as Buffer).length * 1000) / throttle));
      }
      return Buffer.concat(chunks);
    };
    const ctxFor = (body: Buffer): Ctx => ({ req, res, url, body, form: method === "POST" ? parseForm(req, body) : [], query: [...url.searchParams.entries()] });
    const r = this.find(method, url.pathname, this.safeMode);
    if (url.pathname.startsWith("/__sim/")) {   // test endpoints: no guard, admission, latency or log
      const ctx = ctxFor(await readBody(0));
      if (!r) return sendJson(ctx, 404, { error: "not_found" });
      return void (await r.handler(this, ctx));
    }
    const isUpload = method === "POST" && url.pathname === "/api/system/update";
    const entry = { method, path: url.pathname + url.search, body: "", t0: Date.now(), t1: 0 };
    this.requestLog.push(entry);
    if (this.uploadInFlight > 0 && !isUpload) this.duringUpload++;
    this.inFlight++;
    if (isUpload) this.uploadInFlight++;
    this.maxInFlight = Math.max(this.maxInFlight, this.inFlight);
    let admitted = false;
    const epoch = this.epoch;
    res.on("close", () => {
      if (epoch !== this.epoch) return;   // counters were reset with the scenario
      this.inFlight--;
      if (isUpload) this.uploadInFlight--;
      if (admitted) this.admittedInFlight--;
      entry.t1 = Date.now();
    });
    // As the device (AsyncWebServer answers once the request is in): the body is read to its end first, so an
    // upload refused with 503 is not cut off mid-body (browsers report that as a network error)
    const busy = (verdict: string) => {
      req.on("end", () => {
        res.writeHead(503, { "Retry-After": "1", "X-GBSC-Busy": verdict, ...(url.pathname === "/" ? { "Content-Type": "text/html" } : {}) });
        res.end(url.pathname === "/" ? "<script>setTimeout(()=>location.reload(),2000)</script>GBSC PRO is busy, retrying..." : "");
      });
      req.resume();
    };
    if (this.faults.has("busyOnce") && url.pathname.startsWith("/api/")) { this.faults.delete("busyOnce"); return busy("load"); }
    if (this.faults.has("busy503All") || (this.faults.has("busy503") && url.pathname.startsWith("/api/"))) return busy("load");
    if (!exempt(method, url.pathname)) {
      const v = this.admission.decide(this.state.heap, this.state.maxBlock, this.admittedInFlight + 1,
                                      this.state.upload.state === "receiving", isUpload);
      if (v !== "admit") return busy(v);
      admitted = true;
      this.admittedInFlight++;
    }
    if (isUpload) this.state.upload = { state: "receiving", error: null, received: 0, total: Number(req.headers["content-length"] || 0) };
    const prof = PROFILES[this.net];
    const body = await readBody(prof.bytesPerSec);
    entry.body = body.length < 2048 ? body.toString("utf8") : `<${body.length} bytes>`;
    const ctx = ctxFor(body);
    const extra = String(this.state.values.get("system.extra_hosts") ?? "");
    const guardOk = hostAllowed(req.headers.host, extra) &&
                    (!req.headers.origin || originAllowed(req.headers.origin, extra)) &&
                    !(method !== "GET" && url.pathname.startsWith("/api/") && req.headers["x-gbsc"] !== "1");
    if (!guardOk) {
      if (isUpload) this.state.upload.state = "idle";
      return sendJson(ctx, 403, { error: "forbidden", hint: "open the device by IP or http://gbscontrol.local/ (other names: system.extra_hosts, serial command H)" });
    }
    if (prof.latencyMs) await sleep(prof.latencyMs);
    if (prof.bytesPerSec) {   // response throughput: the end of the answer waits for its bytes
      const end = res.end.bind(res) as (...a: unknown[]) => http.ServerResponse;
      (res as unknown as { end: (...a: unknown[]) => void }).end = (...a: unknown[]) => {
        const chunk = a[0];
        const n = typeof chunk === "string" ? Buffer.byteLength(chunk) : chunk instanceof Buffer ? chunk.length : 0;
        setTimeout(() => end(...a), Math.round((n * 1000) / prof.bytesPerSec));
      };
    }
    if (!r) { if (isUpload) this.state.upload.state = "idle"; return sendJson(ctx, 404, { error: "not_found" }); }
    await r.handler(this, ctx);
  }

  async start(port: number, wsPort: number): Promise<void> {
    this.wsPort = await this.ws.start(wsPort);
    await new Promise<void>((ok) => this.http.listen(port, "127.0.0.1", () => ok()));
    const p = (this.http.address() as AddressInfo).port;
    this.url = `http://127.0.0.1:${p}`;
    this.wsUrl = `ws://127.0.0.1:${this.wsPort}/`;
    // settingsBroadcastStatus every ~973 ms (gbs-control.ino loop)
    this.timers.push(setInterval(() => {
      this.ws.broadcast(`s:status.rev=${this.state.settingsRev}`);
      this.ws.broadcast(`s:status.preset=${this.state.presetChar()}`);
    }, 973));
  }

  push(msg: string): void { this.ws.broadcast(msg); }

  // Resolves when every accepted write has been applied (tests: the device applies after answering)
  async idle(): Promise<void> {
    while (this.pendingApplies > 0) await new Promise((r) => setTimeout(r, 2));
  }

  async stop(): Promise<void> {
    for (const t of this.timers) clearInterval(t);
    await this.ws.stop();
    this.http.closeAllConnections?.();
    await new Promise<void>((ok) => this.http.close(() => ok()));
  }
}

// Developer actions and the /api/live value they change (menu.json Developer page)
const DEV_FLIPS: Record<string, string> = {
  "dev.syncwatcher": "syncwatcher", "dev.invert_sync": "sync_polarity", "dev.if_auto_offset": "if_offset", "dev.debug_view": "debug_view",
  "dev.freeze_capture": "freeze", "dev.osd_test": "osd_test", "dev.osd_always_on": "osd_always_on", "dev.print_info": "print_info",
  "dev.ir_codes": "ir_log", "dev.enable_ota": "ota",
};
const DEV_STEPS: Record<string, [string, number]> = {
  "dev.htotal_up": ["htotal", 1], "dev.htotal_down": ["htotal", -1], "dev.pll_div_up": ["pll_div", 1], "dev.pll_div_down": ["pll_div", -1],
};
const OTA_CHANNELS = ["stable", "beta"];   // event a: channel index (ota-plan.h)

export async function startSim(opts: SimOptions = {}): Promise<Sim> {
  const sim = new Sim(opts);
  await sim.start(opts.port ?? 8080, opts.wsPort ?? 8081);
  return sim;
}

export function routeHandled(sim: Sim, method: string, path: string, safe: boolean): boolean {
  return !!sim.find(method, path, safe);
}

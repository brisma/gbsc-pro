// /api/wifi, /api/wifi/scan, /api/wifi/connect, /api/wifi/reset (api-web.cpp)
import type { Ctx } from "./http.ts";
import { arg, sendJson } from "./http.ts";
import type { Sim } from "./server.ts";

export function wifiGet(sim: Sim, ctx: Ctx): void {
  const w = sim.state.wifi;
  sendJson(ctx, 200, { mode: w.mode, ssid: w.mode === "AP" ? "gbscontrol" : w.ssid, ip: w.mode === "AP" ? "192.168.4.1" : w.ip,
                       hostname: "gbscontrol.local", extra_hosts: String(sim.state.values.get("system.extra_hosts") ?? ""),
                       ap_password: w.mode === "AP" ? "qqqqqqqq" : null });
}

export function wifiScanGet(sim: Sim, ctx: Ctx): void {
  const w = sim.state.wifi;
  if (!w.scanned) { w.scanned = true; return sendJson(ctx, 200, { scanning: 1 }); }
  sendJson(ctx, 200, { networks: [{ ssid: "Home", quality: 90, secure: 1 }, { ssid: "Cafe", quality: 40, secure: 0 }, { ssid: "Neighbour 5G", quality: 20, secure: 1 }] });
}

export function wifiConnectPost(sim: Sim, ctx: Ctx): void {
  const ssid = arg(ctx, "ssid");
  if (!ssid || ssid.length > 32) return sendJson(ctx, 400, { ok: 0, errors: { ssid: "invalid" } });
  sim.state.log("wifi", "info", `connecting to ${ssid}`);
  sim.state.wifi = { ...sim.state.wifi, mode: "STA", ssid };
  sendJson(ctx, 200, { ok: 1 });
}

export function wifiResetPost(sim: Sim, ctx: Ctx): void {
  sendJson(ctx, 200, { ok: 1 });
  sim.state.wifi.mode = "AP";
  sim.state.log("wifi", "info", "reset: hotspot after the restart");
  sim.restart();
}

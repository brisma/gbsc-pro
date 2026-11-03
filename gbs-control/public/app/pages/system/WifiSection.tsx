// System › WiFi: mode (Hotspot / Your WiFi), network, address, hostname, the hotspot password (AP only), Extra hosts
// (Advanced, the device's rule as the field error, in_use explained), reset to hotspot; Networks: Scan in the band
// (polled while the device answers "scanning"), each network with its signal bars and Join (/api/wifi*, api-web.cpp)
import { useEffect, useState } from "preact/hooks";
import { signal } from "@preact/signals";
import { Icon } from "../../art/icon.tsx";
import { device, mode } from "../../store/app.ts";
import { Frame } from "../../rows/Frame.tsx";
import { RowView } from "../../rows/Row.tsx";
import { confirm } from "../../shell/Confirm.tsx";
import { openSheet, closeSheet, SheetButtons } from "../../shell/Sheet.tsx";
import { hostsProblem } from "../../model/hosts.ts";
import type { Row } from "../../layout/types.ts";
import { Seg } from "../../rows/controls.tsx";
import { SysGroup, MsgRow } from "./parts.tsx";

interface Network { ssid: string; quality: number; secure: number }
type Scan = { state: "idle" } | { state: "scanning" } | { state: "done"; networks: Network[] } | { state: "error"; text: string };
const scan = signal<Scan>({ state: "idle" });   // kept while the section closes and opens again
const SCAN_POLL_MS = 1000, SCAN_TRIES = 20;
const AP_SSID = "gbscontrol", AP_IP = "192.168.4.1";

// The TV menu's WiFi rows (menu.json ids) with their icons
const ROWS: Record<string, Row> = {
  mode: { kind: "live", ref: "menu:system/wifi/mode", icon: "router" },
  networkAp: { kind: "live", ref: "menu:system/wifi/network", icon: "wifi_tethering" },
  network: { kind: "live", ref: "menu:system/wifi/network", icon: "wifi" },
  address: { kind: "live", ref: "menu:system/wifi/address", icon: "location_on" },
  password: { kind: "live", ref: "menu:system/wifi/password", icon: "key" },
  hostname: { kind: "live", ref: "menu:system/wifi/hostname", icon: "dns" },
  hosts: { kind: "live", ref: "menu:system/wifi/extra-hosts", icon: "dns" },
  reset: { kind: "action", ref: "menu:system/wifi/reset-to-hotspot", icon: "wifi_off" },
  optimize: { kind: "toggle", ref: "menu:system/wifi/optimize-wifi", icon: "network_check" },
};
const log = (m: string) => console.debug("[wifi] " + m);

async function runScan(): Promise<void> {
  if (scan.value.state === "scanning") return;
  scan.value = { state: "scanning" };
  log("scan started");
  for (let i = 0; i < SCAN_TRIES; i++) {
    try {
      const j = await device.client.getJson<{ scanning?: number; networks?: Network[] }>("/api/wifi/scan");
      if (j.networks) { scan.value = { state: "done", networks: j.networks }; log(`scan: ${j.networks.length} network(s)`); return; }
    } catch (e) { log(`scan: ${String(e)}`); }
    await new Promise((ok) => window.setTimeout(ok, SCAN_POLL_MS));
  }
  scan.value = { state: "error", text: "The scan did not finish: try again" };
}

async function resetHotspot(): Promise<void> {
  if (!(await confirm({ icon: "wifi_off", title: "Reset to hotspot?", ok: "Reset",
    body: `GBSC PRO forgets your WiFi network and restarts as the hotspot "${AP_SSID}" (address ${AP_IP}).` }))) return;
  const res = await device.client.post("/api/wifi/reset");
  log(`reset to hotspot: HTTP ${res.status}`);
  if (res.ok) device.toast(`Restarting as the hotspot ${AP_SSID}`, "info");
  else device.toast(`Not reset (HTTP ${res.status})`, "error");
}

function ConnectForm({ n }: { n: Network }) {
  const [pass, setPass] = useState("");
  const [state, setState] = useState<"form" | "sending" | "sent" | "error">("form");
  const go = async () => {
    setState("sending");
    const form: Record<string, string> = { ssid: n.ssid };
    if (n.secure) form.password = pass;
    const res = await device.client.post("/api/wifi/connect", form);
    log(`connect to ${n.ssid}: HTTP ${res.status}`);
    setState(res.ok ? "sent" : "error");
  };
  if (state === "sent")
    return (
      <>
        <p>GBSC PRO is joining <b>{n.ssid}</b>. If this page stops answering, connect your phone or computer to {n.ssid} and open <b>gbscontrol.local</b> in the browser (or the address the TV menu shows under System › WiFi).</p>
        <SheetButtons><button type="button" class="btn" onClick={closeSheet}>Close</button></SheetButtons>
      </>
    );
  const short = n.secure && pass.length < 8;
  return (
    <>
      {n.secure ? (
        <div class="field">
          <label for="wpass">Password</label>
          <input id="wpass" class="in" type="password" autocomplete="current-password" maxLength={63} aria-describedby="wpass-h" value={pass}
                 onInput={(e) => setPass((e.target as HTMLInputElement).value)} />
          <small id="wpass-h" class="hint">8 to 63 characters</small>
        </div>
      ) : <p>{n.ssid} is an open network: anyone nearby can see what goes over it.</p>}
      {state === "error" ? <p class="bad">The device refused the network name.</p> : null}
      <SheetButtons>
        <button type="button" class="btn ghost" onClick={closeSheet}>Cancel</button>
        <button type="button" class="btn pri" disabled={short || state === "sending"} onClick={() => void go()}>Connect</button>
      </SheetButtons>
    </>
  );
}

function HostsForm({ now }: { now: string }) {
  const [v, setV] = useState(now);
  const [err, setErr] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);
  const rule = hostsProblem(v);
  const save = async () => {
    setBusy(true);
    const res = await device.client.post("/api/settings", { "system.extra_hosts": v });
    setBusy(false);
    log(`extra hosts "${v}": HTTP ${res.status}`);
    if (res.ok) {   // the device applies it in its loop: show it now, read /api/wifi again a moment later
      if (device.wifi.value) device.wifi.value = { ...device.wifi.value, extra_hosts: v };
      closeSheet();
      window.setTimeout(() => void device.loadWifi(), 800);
      return;
    }
    let code = "";
    try { code = String((await res.json()).error ?? ""); } catch { /* no JSON */ }
    setErr(code === "in_use"
      ? "Not saved: this page was opened with one of the names you removed, and it would stop working. Open the page by its address first, then remove the name."
      : `Not saved: the device refused the list (HTTP ${res.status}).`);
  };
  return (
    <>
      <p>Other names that open this page, as your router's DNS knows them, separated by commas. The address and gbscontrol.local always work.</p>
      <div class="field">
        <label for="xhosts">Names</label>
        <input id="xhosts" class="in" type="text" spellcheck={false} autocapitalize="off" value={v} aria-invalid={!!rule} aria-describedby="xhosts-h"
               onInput={(e) => { setV((e.target as HTMLInputElement).value); setErr(null); }} />
        <small id="xhosts-h" class={rule ? "hint bad" : "hint"}>{rule ?? "Up to 63 characters in all"}</small>
      </div>
      {err ? <p class="bad" role="alert">{err}</p> : null}
      <SheetButtons>
        <button type="button" class="btn ghost" onClick={closeSheet}>Cancel</button>
        <button type="button" class="btn pri" disabled={!!rule || v === now || busy} onClick={() => void save()}>Save</button>
      </SheetButtons>
    </>
  );
}

const SCAN_ICONS: Record<string, string> = { scanning: "refresh", idle: "travel_explore", error: "travel_explore" };   // @icons
const scanIcon = (s: string) => SCAN_ICONS[s] ?? "travel_explore";

function Bars({ q }: { q: number }) {
  const n = q >= 75 ? 4 : q >= 50 ? 3 : q >= 25 ? 2 : 1;
  return <span class="bars" role="img" aria-label={`Signal ${n} of 4`}>{[1, 2, 3, 4].map((i) => <i class={i <= n ? "on" : ""} style={`height:${25 * i}%`} />)}</span>;
}

function NetworkRow({ n, connected }: { n: Network; connected: boolean }) {
  return (
    <div class="rw list net" data-row={`network:${n.ssid}`}>
      <span class="ic"><Icon name={n.secure ? "lock" : "wifi"} /></span>
      <span class="lb"><span>{n.ssid}</span></span>
      <span class="ds">{n.secure ? "Secured" : "Open"}</span>
      <div class="c">
        <Bars q={n.quality} />
        {connected ? <span class="tag ok lg">Connected</span>
          : <button type="button" class="btn" aria-label={`Join ${n.ssid}`}
                    onClick={(e) => openSheet({ title: n.ssid, icon: n.secure ? "lock" : "wifi", body: () => <ConnectForm n={n} /> }, e.currentTarget)}>Join</button>}
      </div>
    </div>
  );
}

export function WifiSection(_: { desk: boolean }) {
  useEffect(() => { void device.loadWifi(); }, []);
  const [showPw, setShowPw] = useState(false);
  const w = device.wifi.value;
  const ap = w?.mode === "AP";
  const adv = mode.value !== "easy";
  // /api/info wifi.slow: Optimize WiFi found every memory clock slow with this picture on the router's channel
  const iw = (device.info.value?.wifi ?? {}) as { slow?: number; channel?: number };
  const slowOn = !ap && iw.slow ? Number(iw.channel ?? 0) : 0;
  const sc = scan.value;
  // a long network name or hostname ends with an ellipsis (the full text in the tooltip): it must not squeeze the label
  const val = (t: string | null | undefined) => <span class="val trunc" title={t ?? ""}><span class="tx">{t ?? ""}</span></span>;
  return (
    <>
      <SysGroup title="Connection">
        <Frame row={ROWS.mode} label="Mode" reason={null} wide sub="Hotspot: the GBSC PRO makes its own network. Your WiFi: it joins yours.">
          <Seg label="Mode" opts={[
            { label: "Hotspot", on: ap, pick: () => { if (!ap) void resetHotspot(); } },
            { label: "Your WiFi", on: !ap, pick: () => { if (ap) void runScan(); } },
          ]} />
        </Frame>
        <Frame row={ap ? ROWS.networkAp : ROWS.network} label="Network" reason={null}>{val(w?.ssid)}</Frame>
        <Frame row={ROWS.address} label="Address" reason={null}>{val(w?.ip)}</Frame>
        {ap ? (
          <Frame row={ROWS.password} label="Password" reason={null}>
            <span class="val">{showPw ? w?.ap_password : "••••••••"}</span>
            <button type="button" class="btn ico" aria-label={showPw ? "Hide the password" : "Show the password"} onClick={() => setShowPw(!showPw)}>
              <Icon name={showPw ? "visibility_off" : "visibility"} />
            </button>
          </Frame>
        ) : null}
        <Frame row={ROWS.hostname} label="Hostname" reason={null}>{val(w?.hostname)}</Frame>
        {adv ? <RowView row={ROWS.optimize} /> : null}
        {slowOn ? <MsgRow icon="router" text={`The link is slow on WiFi channel ${slowOn}: with this picture the scaler disturbs it. Set another 2.4 GHz channel on your router (1, 6 or 11).`} /> : null}
        {adv ? (
          <Frame row={ROWS.hosts} label="Extra hosts" reason={null}>
            <button type="button" class="val trunc lnkv" aria-label={`Extra hosts: ${w?.extra_hosts || "none"}`}
                    onClick={(e) => openSheet({ title: "Extra hosts", icon: "dns", body: () => <HostsForm now={w?.extra_hosts ?? ""} /> }, e.currentTarget)}>
              <span class="tx">{w?.extra_hosts || "None"}</span><Icon name="chevron_right" />
            </button>
          </Frame>
        ) : null}
        <Frame row={ROWS.reset} label="Reset to hotspot" reason={null} sub="Forgets your WiFi and goes back to the hotspot.">
          <button type="button" class="btn danger" onClick={() => void resetHotspot()}>Reset</button>
        </Frame>
      </SysGroup>
      <SysGroup title="Networks" action={
        <button type="button" class="btn" disabled={sc.state === "scanning"} onClick={() => void runScan()}><Icon name="refresh" />{sc.state === "scanning" ? "Scanning…" : "Scan"}</button>}>
        {sc.state === "done"
          ? (sc.networks.length ? sc.networks.map((n) => <NetworkRow key={n.ssid} n={n} connected={!ap && n.ssid === w?.ssid} />)
            : <MsgRow icon="wifi_find" text="No network found" />)
          : <MsgRow icon={scanIcon(sc.state)} text={sc.state === "scanning" ? "Looking for networks…" : sc.state === "error" ? sc.text : "Scan to list the networks around the device"} />}
      </SysGroup>
    </>
  );
}

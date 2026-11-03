// Named device situations for the tests (POST /__sim/scenario {"name"}); each starts from a fresh device
import type { Sim } from "./server.ts";
import type { Board } from "./types.ts";

export const SCENARIOS = ["pro-rgbs", "pro-svideo-ntsc", "pro-composite-pal", "pro-composite-ntsc", "pro-vga", "pro-component",
  "pro-no-signal", "pro-auto-search", "standard-rgbs", "standard-vga", "unsaved-profile", "update-offer", "adv-update-offer",
  "adv-absent", "adv-old-image", "profiles-full", "safe-mode", "low-memory", "long-texts"];

export function applyScenario(sim: Sim, name: string): boolean {
  if (!SCENARIOS.includes(name)) return false;
  const board: Board = name.startsWith("standard-") ? "standard" : "pro";
  sim.reset(board);
  const st = sim.state, rt = st.runtime;
  const input = (n: number) => { rt.activeInput = n; st.values.set("input.last", n); };
  switch (name) {
    case "pro-rgbs": case "standard-rgbs": input(1); break;
    case "pro-svideo-ntsc": input(5); rt.advNtsc = true; rt.std = 0; rt.interlaced = false; break;
    case "pro-composite-pal": input(6); rt.advNtsc = false; rt.srcHz10 = 500; rt.srcLines = 288; rt.std = 4; break;
    case "pro-composite-ntsc": input(6); rt.advNtsc = true; rt.std = 0; break;
    case "pro-vga": case "standard-vga": input(3); rt.srcLines = 480; rt.std = 8; break;
    case "pro-component": input(4); break;
    case "pro-no-signal": rt.noSignal = true; rt.scanlinesOk = false; rt.srcHz10 = 0; rt.outHz10 = 0; break;
    case "pro-auto-search": rt.searching = true; rt.noSignal = true; rt.scanlinesOk = false; break;
    case "unsaved-profile":
      sim.profiles.run("saveas", -1, "Saturn", "NTSC-J");
      sim.profiles.modified = true;
      break;
    case "update-offer": st.updateOffer = { version: "3.0.1", channel: "stable" }; st.updateLast = "3.0.1"; st.serverOffer = st.updateOffer; break;
    case "adv-update-offer": st.advUpdate = true; break;
    case "adv-absent": st.advStatus = "no_reply"; break;
    case "adv-old-image": st.advCaps = ["signal"]; break;
    case "profiles-full": sim.profiles.fill(36); break;
    case "safe-mode": sim.safeMode = true; break;
    case "low-memory": st.heap = 9000; st.maxBlock = 4000; break;
    case "long-texts":
      for (const n of ["Super Street Fighter II T", "Castlevania Bloodlines EU", "Phantasy Star IV The End"]) sim.profiles.run("saveas", -1, n.slice(0, 24), "PAL");
      st.wifi.ssid = "A very long network name 32 chr";
      st.values.set("system.extra_hosts", "living-room-scaler.home.lan,gbs-pro-cabinet.fritz.box");
      st.log("sync", "info", "x".repeat(40) + " a long line that the console must wrap without widening the page " + "y".repeat(24));
      break;
  }
  return true;
}

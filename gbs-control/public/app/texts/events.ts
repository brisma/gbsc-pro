// The Status history's words for the firmware's event ring (pro/log/event-ring.*, GET /api/events), as implemented:
// input-auto-device.cpp:140-178, profile-store.cpp:583, gbs-control.ino:3817, ota-update.cpp:73, update-device.cpp:206
export interface EventEntry { seq: number; ms: number; type: string; a: number; b: number }
export interface EventCtx { inputName: (input: number) => string; profileName: (id: string) => string }
export interface EventView { title: string; sub: string; icon: string; cls: "" | "g" | "w" }

const CHANNELS = ["Stable", "Beta"];
const PRESETS: Record<number, string> = { 1: "960p", 2: "1024p", 3: "720p", 4: "480p", 5: "1080p", 6: "Downscale", 7: "1200p" };

// rto->presetID: low digit the output, 0x10 a PAL source (480p becomes 576p), from 0x21 the bypass presets
export function presetName(id: number): string {
  if (id >= 0x21) return "Bypass";
  const pal = (id & 0xf0) === 0x10, low = id & 0x0f;
  if (low === 4) return pal ? "576p" : "480p";
  return PRESETS[low] ?? `preset ${id.toString(16)}`;
}

export function eventText(e: EventEntry, c: EventCtx): EventView {
  const pid = "p" + String(e.a).padStart(2, "0");
  const prof = () => c.profileName(pid) || pid;
  switch (e.type) {
    case "input_found": return { title: `Input found: ${c.inputName(e.a)}`, sub: e.b ? "fallback: no other cable answered" : "found by Auto", icon: "cable", cls: "" };
    case "input_lost": return { title: `Signal lost on ${c.inputName(e.a)}`, sub: "Auto looks again", icon: "warning", cls: "w" };
    case "input_changed": return { title: `Input changed: ${c.inputName(e.a)}`, sub: e.b ? "by hand" : "chosen by the RGB/Composite judge", icon: "swap_horiz", cls: "" };
    case "profile_loaded": return { title: `Profile loaded: ${prof()}`, sub: "", icon: "check_circle", cls: "g" };
    case "profile_saved": return { title: `Profile saved: ${prof()}`, sub: "", icon: "save", cls: "g" };
    case "resolution": return { title: `Resolution: ${presetName(e.a)}`, sub: `${(e.a & 0xf0) === 0x10 ? "PAL source" : "60 Hz source"} on ${c.inputName(e.b)}`, icon: "hd", cls: "" };
    case "update_offered": return { title: `Update offered (${CHANNELS[e.a] ?? "channel " + e.a})`, sub: "see System › Update", icon: "system_update_alt", cls: "" };
    case "update_done": return { title: "Firmware installed", sub: e.a ? "downloaded by the device" : "from a file upload", icon: "system_update_alt", cls: "g" };
    default: return { title: e.type, sub: "", icon: "info", cls: "" };
  }
}

// "now", "12 s ago", "3 min ago", "2 h ago": event ms against the device uptime now
export function ago(ms: number, nowMs: number): string {
  const s = Math.max(0, Math.round((nowMs - ms) / 1000));
  if (s < 5) return "now";
  if (s < 60) return `${s} s ago`;
  if (s < 3600) return `${Math.floor(s / 60)} min ago`;
  return `${Math.floor(s / 3600)} h ago`;
}

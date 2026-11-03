// Profiles store of the simulator: the JSON of profilesJson, the ops of profile-store.cpp with their results on the
// WebSocket (status.profile_op, status.profile, status.profile_modified, status.profiles_rev)
import type { DeviceState } from "./state.ts";

export const PROFILE_MAX = 36;
export const REGIONS = ["NTSC-U", "NTSC-J", "PAL", "PAL60", "None"];
export interface Profile { name: string; region: string; hasFp: boolean; fpInput: number; fpLines: number; lastused: number; modes: string[]; settings: Record<string, number> }

const id = (i: number) => "p" + String(i).padStart(2, "0");
export const indexOfId = (s: string | null) => (s && /^p\d\d$/.test(s) && Number(s.slice(1)) < PROFILE_MAX ? Number(s.slice(1)) : -1);
export const sanitizeName = (s: string | null) => { const t = (s ?? "").trim().slice(0, 24); return t.length ? t : null; };

export class ProfileStore {
  slots: (Profile | null)[] = new Array(PROFILE_MAX).fill(null);
  active = -1;
  modified = false;
  rev = 1;
  private sent = { active: "", modified: "", rev: "" };
  private state: DeviceState;
  private clock = 1000;

  constructor(state: DeviceState) { this.state = state; }

  fill(n: number): void {
    for (let i = 0; i < n && i < PROFILE_MAX; i++)
      this.slots[i] = { name: `Game ${i + 1}`, region: "None", hasFp: false, fpInput: 0, fpLines: 0, lastused: this.clock++, modes: ["ntsc"], settings: {} };
    this.rev++;
  }

  private profileKeys(): string[] {
    return this.state.gen.registry.settings.filter((e) => e.scope === "profile" && e.type !== "str" && this.state.values.has(e.key)).map((e) => e.key);
  }

  private capture(): Record<string, number> {
    const out: Record<string, number> = {};
    for (const k of this.profileKeys()) out[k] = this.state.num(k);
    return out;
  }

  json(): unknown {
    const order = this.slots.map((p, i) => ({ p, i })).filter((x) => x.p).sort((a, b) => a.p!.name.localeCompare(b.p!.name));
    return {
      active: this.active >= 0 ? id(this.active) : "", modified: this.modified ? 1 : 0, autoload: this.state.num("profiles.autoload"),
      base: this.active >= 0 && this.state.num("output.preset") !== 2 ? this.state.num("output.preset") : 0,
      profiles: order.map(({ p, i }) => ({
        id: id(i), name: p!.name, region: p!.region,
        fp: p!.hasFp ? `${["", "RGB SCART", "RGB SoG", "VGA", "Component", "S-Video", "Composite"][p!.fpInput] || "?"} · ${p!.fpLines} lines · NTSC` : "",
        lastused: p!.lastused, match: this.matches(p!) ? 1 : 0, modes: p!.modes,
      })),
    };
  }

  // profileMatches (profile-autoload.cpp): same input, lines within 4 (the mode test is folded into the lines here);
  // none without a signal
  private matches(p: Profile): boolean {
    const rt = this.state.runtime;
    return !rt.noSignal && p.hasFp && p.fpInput === rt.activeInput && Math.abs(p.fpLines - rt.srcLines) <= 4;
  }

  exportText(i: number): string {
    const p = this.slots[i]!;
    let t = `schema=1\nprofile.name=${p.name}\nprofile.region=${p.region}\nprofile.modes=${p.modes.join(",")}\n`;
    if (p.hasFp) t += `profile.fp.input=${p.fpInput}\nprofile.fp.lines=${p.fpLines}\n`;
    for (const [k, v] of Object.entries(p.settings)) t += `${k}=${v}\n`;
    return t;
  }

  private freeSlot(): number { return this.slots.findIndex((p) => !p); }

  // Called after every applied setting write: a profile-scope change marks the active profile modified
  settingChanged(key: string): void {
    const e = this.state.entry(key);
    if (this.active >= 0 && e?.scope === "profile" && !this.modified) { this.modified = true; this.statusTick(); }
  }

  statusTick(): void {
    const a = this.active >= 0 ? this.slots[this.active]?.name ?? "" : "";
    const m = this.modified ? "1" : "0";
    const r = String(this.rev);
    if (a !== this.sent.active) { this.sent.active = a; this.state.send(`s:status.profile=${a}`); }
    if (m !== this.sent.modified) { this.sent.modified = m; this.state.send(`s:status.profile_modified=${m}`); }
    if (r !== this.sent.rev) { this.sent.rev = r; this.state.send(`s:status.profiles_rev=${r}`); }
  }

  // One op; returns the error reason (profile-store.cpp words) or null
  run(kind: string, i: number, name: string | null, region: string | null, text?: string): string | null {
    const st = this.state;
    switch (kind) {
      case "load": {
        const p = this.slots[i]; if (!p) return "unknown";
        for (const [k, v] of Object.entries(p.settings)) st.values.set(k, v);
        st.settingsRev++; this.active = i; this.modified = false; p.lastused = this.clock++; break;
      }
      case "save": { const p = this.slots[i]; if (!p) return "unknown"; p.settings = this.capture(); if (this.active === i) this.modified = false; break; }
      case "saveas": {
        const f = this.freeSlot(); if (f < 0) return "limit";
        this.slots[f] = { name: name ?? "Profile", region: region && REGIONS.includes(region) ? region : "None", hasFp: true, fpInput: st.runtime.activeInput,
                          fpLines: st.runtime.srcLines, lastused: this.clock++, modes: ["ntsc"], settings: this.capture() };
        this.active = f; this.modified = false; break;
      }
      case "rename": { const p = this.slots[i]; if (!p) return "unknown"; p.name = name ?? p.name; break; }
      case "delete": { if (!this.slots[i]) return "unknown"; this.slots[i] = null; if (this.active === i) { this.active = -1; this.modified = false; } break; }
      case "duplicate": {
        const p = this.slots[i]; if (!p) return "unknown"; const f = this.freeSlot(); if (f < 0) return "limit";
        this.slots[f] = { ...p, name: (p.name + " 2").slice(0, 24), settings: { ...p.settings }, lastused: this.clock++ }; break;
      }
      case "clear_fp": { const p = this.slots[i]; if (!p) return "unknown"; p.hasFp = false; break; }
      case "defaults": {
        for (const k of this.profileKeys()) { const e = st.entry(k); if (e) st.values.set(k, e.def ?? 0); }
        st.settingsRev++; this.active = -1; this.modified = false; break;
      }
      case "replace": {
        const p = this.slots[i]; if (!p) return "unknown";
        p.settings = this.capture(); p.hasFp = true; p.fpInput = st.runtime.activeInput; p.fpLines = st.runtime.srcLines;
        if (name) { p.name = name; p.region = region && REGIONS.includes(region) ? region : "None"; }   // profileReplace
        this.active = i; this.modified = false; break;
      }
      case "import": {
        const f = this.freeSlot(); if (f < 0) return "limit";
        const get = (k: string) => new RegExp(`^${k.replace(".", "\\.")}=(.*)$`, "m").exec(text ?? "")?.[1];
        const nm = sanitizeName(get("profile.name") ?? null); if (!nm) return "invalid";
        const settings: Record<string, number> = {};
        for (const line of (text ?? "").split("\n")) {
          const m = /^([a-z0-9_.]+)=(\d+)$/.exec(line.trim());
          if (m && !m[1].startsWith("profile.") && st.entry(m[1])?.scope === "profile") settings[m[1]] = Number(m[2]);
        }
        this.slots[f] = { name: nm, region: get("profile.region") ?? "None", hasFp: !!get("profile.fp.input"), fpInput: Number(get("profile.fp.input") ?? 0),
                          fpLines: Number(get("profile.fp.lines") ?? 0), lastused: this.clock++, modes: (get("profile.modes") ?? "ntsc").split(","), settings };
        break;
      }
      default: return "unknown";
    }
    this.rev++;
    return null;
  }
}

// The unit on the network: found by mDNS (gbscontrol.local) or typed (IP address), shared by the WiFi areas
import { signal } from "@preact/signals";
import { Icon } from "../../../gbs-control/public/app/art/icon.tsx";
import { api, errText, type UnitRow } from "./api.ts";
import { busy, host, note, setHost, unit } from "./store.ts";
import { cleanHost, notFoundHint } from "./state.ts";

const WINDOWS = navigator.userAgent.includes("Windows");

export { unit };
const found = signal<string[] | null>(null);
const searching = signal(false);
const connectError = signal<string | null>(null);

/** Connects to `h`; true when the unit answered (an earlier search's "not found" goes then). */
export async function connect(typed: string): Promise<boolean> {
  const h = cleanHost(typed);
  setHost(h); unit.value = null; connectError.value = null;
  try {
    const u = await api.unitInfo(h);
    if (host.value !== h) return false;
    unit.value = u; found.value = null; return true;
  } catch (e) { note(errText(e)); if (host.value === h) connectError.value = errText(e); return false; }
}

export const unitSummary = (u: UnitRow) => `${host.value} · GBS-Control ${u.fw}${u.advVersion ? ` · ADV Controller ${u.advVersion}` : ""}${u.safeMode ? " · safe mode" : ""}`;

/** Safe mode: only the install, the backup download and the log answer (shown where those buttons are). */
export function SafeModeNote() {
  return unit.value?.safeMode
    ? <div class="warnbox"><Icon name="warning" /><span>The GBSC is in safe mode: its firmware crashed 3 times in a row. Install a GBS-Control image to leave it; until then only the install and the backup download work.</span></div>
    : null;
}

/** `onConnected`: the step that holds the picker closes once a unit answers (Connect after Change). */
export function UnitPicker({ onConnected }: { onConnected?: () => void }) {
  const join = async (h: string) => { if (await connect(h)) onConnected?.(); };
  const search = async () => { searching.value = true; found.value = await api.findUnits().catch(() => []); searching.value = false; if (found.value.length === 1) join(found.value[0]); };
  return (
    <div class="stack">
      <input value={host.value} onInput={(e) => setHost((e.target as HTMLInputElement).value)} placeholder="IP address, e.g. 192.168.1.50" aria-label="GBSC address" />
      <div class="pair">
        <button class="btn" disabled={!host.value || !!busy.value} onClick={() => join(host.value)}><Icon name="link" />Connect</button>
        <button class="btn" disabled={searching.value || !!busy.value} onClick={search}><Icon name="search" />{searching.value ? "Searching" : "Find on the network"}</button>
      </div>
      {found.value && found.value.length !== 1 && <p class="hint">{found.value.length ? "Several answers, choose one:" : notFoundHint(WINDOWS)}</p>}
      {found.value && found.value.length > 1 && <div class="pair">{found.value.map((h) => <button class="btn" onClick={() => join(h)}>{h}</button>)}</div>}
      {connectError.value && <div class="warnbox"><Icon name="error" /><span>{connectError.value}</span></div>}
    </div>
  );
}

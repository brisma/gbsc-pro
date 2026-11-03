// Shell: wordmark, three areas (USB devices, update over WiFi, backup and profiles), the job log and confirmations
import { useEffect } from "preact/hooks";
import { signal } from "@preact/signals";
import { Logo } from "../../../gbs-control/public/app/art/logo.tsx";
import { Icon } from "../../../gbs-control/public/app/art/icon.tsx";
import { onProgress } from "./api.ts";
import { asking, busy, clearJob, log, onProgressEvent, pct } from "./store.ts";
import { Devices } from "./devices.tsx";
import { Update } from "./update.tsx";
import { Backup } from "./backup.tsx";

type Area = "usb" | "wifi" | "backup";
const area = signal<Area>("usb");
const AREAS: { id: Area; label: string; icon: string }[] = [
  { id: "usb", label: "USB", icon: "usb" },
  { id: "wifi", label: "WiFi", icon: "wifi" },
  { id: "backup", label: "Backup", icon: "inventory_2" },
];

function Job() {
  if (!busy.value && !log.value.length) return null;
  return (
    <div class="job">
      {pct.value !== null && <div class="bar"><i style={{ width: `${pct.value}%` }} /></div>}
      <div class="lines mono">{log.value.slice(-6).map((l) => <div>{l}</div>)}</div>
    </div>
  );
}

function Confirm() {
  const a = asking.value;
  if (!a) return null;
  const answer = (yes: boolean) => { asking.value = null; a.resolve(yes); };
  return (
    <div class="scrim" onClick={() => answer(false)}>
      <div class="dlg" role="dialog" aria-modal="true" onClick={(e) => e.stopPropagation()}>
        <h2>{a.title}</h2>
        <p>{a.body}</p>
        <div class="btns"><button class="btn" onClick={() => answer(false)}>Cancel</button><button class="btn pri" onClick={() => answer(true)}>{a.ok}</button></div>
      </div>
    </div>
  );
}

export function App() {
  useEffect(() => { const u = onProgress(onProgressEvent); return () => { u.then((f) => f()); }; }, []);
  return (
    <div class="ui flasher">
      <header class="hd">
        <Logo height={26} />
        <span class="ttl">Flasher</span>
        <nav class="areas">
          {AREAS.map((a) => (
            <button class={area.value === a.id ? "on" : ""} disabled={!!busy.value} onClick={() => { if (area.value !== a.id) clearJob(); area.value = a.id; }}>
              <Icon name={a.icon} />{a.label}
            </button>
          ))}
        </nav>
      </header>
      <main>
        {area.value === "usb" && <Devices />}
        {area.value === "wifi" && <Update />}
        {area.value === "backup" && <Backup />}
      </main>
      <Job />
      <Confirm />
    </div>
  );
}

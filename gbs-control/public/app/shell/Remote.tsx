// The web remote: the IR remote's keys (pro/ui/ir-input.cpp; no Quick key) sent as menu.key with the
// MenuKey code (pro/ui/menu-types.h); arrows and volume hold to repeat
import { signal } from "@preact/signals";
import { Icon } from "../art/icon.tsx";
import { device, board } from "../store/app.ts";
import { openSheet, closeSheet, SheetButtons } from "./Sheet.tsx";
import { useHold } from "./hold.ts";

// enum MenuKey { K_UP, K_DOWN, K_LEFT, K_RIGHT, K_OK, K_BACK, K_MENU, K_INFO, K_SAVE, K_MUTE, K_VOLUP, K_VOLDOWN, ... }
export const KEY = { Up: 0, Down: 1, Left: 2, Right: 3, OK: 4, Exit: 5, Menu: 6, Info: 7, Save: 8, Mute: 9, "Vol +": 10, "Vol −": 11 } as const;
type KeyName = keyof typeof KEY;
const last = signal("");

function send(k: KeyName): void {
  last.value = k;
  const code = KEY[k].toString(16).toUpperCase().padStart(2, "0");
  void device.action("menu.key", code).then((r) => { if (!r.ok) device.toast(`Remote: ${k} not sent (HTTP ${r.status})`, "error"); });
}

function Key({ k, cls, icon, repeat }: { k: KeyName; cls?: string; icon?: string; repeat?: boolean }) {
  const hold = useHold(() => send(k));
  const label = icon ? <Icon name={icon} /> : k;
  return repeat
    ? <button type="button" class={cls} aria-label={k} {...hold}>{label}</button>
    : <button type="button" class={cls} aria-label={k} onClick={() => send(k)}>{label}</button>;
}

function Labelled({ k, icon, repeat }: { k: KeyName; icon: string; repeat?: boolean }) {
  const hold = useHold(() => send(k));
  const inner = <><i><Icon name={icon} /></i>{k}</>;
  return repeat
    ? <button type="button" aria-label={k} {...hold}>{inner}</button>
    : <button type="button" aria-label={k} onClick={() => send(k)}>{inner}</button>;
}

function RemoteBody() {
  return (
    <>
      <p style="font-size:13px">The TV menu, driven from here. Same keys as the IR remote.</p>
      <div class="remote">
        <div class="rpad">
          <Key k="Up" cls="u" icon="keyboard_arrow_up" repeat />
          <Key k="Left" cls="l" icon="keyboard_arrow_left" repeat />
          <Key k="OK" cls="ok" />
          <Key k="Right" cls="r" icon="keyboard_arrow_right" repeat />
          <Key k="Down" cls="d" icon="keyboard_arrow_down" repeat />
        </div>
        <div class="rkeys">
          <Labelled k="Menu" icon="menu" /><Labelled k="Exit" icon="undo" /><Labelled k="Info" icon="info" /><Labelled k="Save" icon="save" />
        </div>
        {board.value === "pro" ? (
          <div class="rkeys"><Labelled k="Vol −" icon="volume_down" repeat /><Labelled k="Mute" icon="volume_off" /><Labelled k="Vol +" icon="volume_up" repeat /></div>
        ) : null}
        <div class="rlast" aria-live="polite">{last.value ? "Sent: " + last.value : " "}</div>
      </div>
      <SheetButtons><button type="button" class="btn" onClick={closeSheet}>Close</button></SheetButtons>
    </>
  );
}

export function openRemote(e?: Event): void {
  last.value = "";
  openSheet({ title: "Remote", icon: "settings_remote", body: () => <RemoteBody /> }, e?.currentTarget);
}

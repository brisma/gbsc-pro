// Kitchen sink of the row components, built only for tests: dist/components.html, served by the simulator at
// /__sim/components. ?mode=advanced switches the browser mode. Every row points at real firmware data.
import { render } from "preact";
import { useEffect } from "preact/hooks";
import { device, desk, mode, ready } from "../store/app.ts";
import { SheetHost } from "../shell/Sheet.tsx";
import { Toasts } from "../shell/Toasts.tsx";
import { GroupView } from "../sections/SectionsPage.tsx";
import type { Group } from "../layout/types.ts";

const GROUPS: Group[] = [
  { title: "Steppers", rows: [
    { kind: "stepper", ref: "menu:picture/brightness", icon: "wb_sunny" },
    { kind: "stepper", ref: "menu:input/sampling/gain", icon: "tune" },
    { kind: "stepper", ref: "menu:developer/sync/hsync-position", icon: "straighten" },
  ] },
  { title: "Choices", rows: [
    { kind: "toggle", ref: "menu:output/always-60hz", icon: "speed" },
    { kind: "inline", ref: "menu:output/hdmi-range", icon: "hd" },
    { kind: "inline", ref: "menu:system/osd-settings/menu-position", icon: "picture_in_picture" },
    { kind: "toggle", ref: "menu:input/2x-smooth", icon: "blur_on" },
  ] },
  { title: "Cards and pickers", rows: [
    { kind: "cards", ref: "menu:picture/filter", icon: "photo_filter", look: "filter" },
    { kind: "cards", ref: "menu:picture/scanlines", icon: "gradient", look: "scanlines" },
    { kind: "picker", ref: "menu:input/source", icon: "cable", look: "source" },
    { kind: "picker", ref: "menu:output/resolution", icon: "hd", look: "resolution" },
  ] },
  { title: "Screen and actions", rows: [
    { kind: "pad", ref: "menu:screen/position", icon: "open_with" },
    { kind: "action", ref: "menu:picture/reset-picture", icon: "restart_alt" },
    { kind: "live", ref: "text:wifi.hostname", icon: "dns", label: "Hostname" },
    { kind: "link", icon: "system_update_alt", label: "Update", to: "#/system/update", sub: "GBS-Control and ADV Controller" },
    { kind: "note", icon: "info", text: "Video standard and 2X appear with S-Video or Composite, as on the TV." },
  ] },
];
const STRIP: Group = { title: "Resolution strip", rows: [{ kind: "picker", ref: "menu:output/resolution", icon: "hd", look: "resolution" }] };

function Kitchen() {
  useEffect(() => {
    if (new URLSearchParams(location.search).get("mode") === "advanced") mode.value = "advanced";
    void device.start();
  }, []);
  return (
    <div class={`ui${desk.value ? " desk" : ""}`} data-ready={ready.value ? "1" : undefined}>
      <div class="col" style="flex:1;display:flex;flex-direction:column;min-width:0;position:relative">
        <main class="list">
          <section class="card">{[...GROUPS, STRIP].map((g) => <GroupView g={g} />)}</section>
        </main>
        <SheetHost /><Toasts />
      </div>
    </div>
  );
}

render(<Kitchen />, document.getElementById("app")!);
document.getElementById("boot")?.remove();   // the shared template's loader (the app removes it when ready)

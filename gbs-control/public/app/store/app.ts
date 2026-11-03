// The app's single device store and the page state shared by the shell and the pages
import { signal, computed, effect } from "@preact/signals";
import registryJson from "../../gen/registry.json" with { type: "json" };
import { createClient } from "../api/client.ts";
import { connectWs } from "../api/ws.ts";
import { createDeviceStore } from "./device.ts";
import { readPref, writePref } from "./persist.ts";
import type { Registry } from "../model/registry.ts";
import type { Mode } from "../model/menu.ts";
import { stateFromStore } from "../model/predicates.ts";
import { offerVersion } from "../model/version.ts";

declare global { interface Window { GBSC_WS_URL?: string } }

const wsUrl = () => window.GBSC_WS_URL ?? "ws://" + location.hostname + ":81/";

export const device = createDeviceStore({
  client: createClient(),
  connect: (url, h) => connectWs(url, h),
  wsUrl: wsUrl(),
  registry: registryJson as unknown as Registry,
  setTimeout: (fn, ms) => window.setTimeout(fn, ms),
  clearTimeout: (h) => window.clearTimeout(h as number),
  now: () => Date.now(),
  visible: () => document.visibilityState !== "hidden",
  log: (m) => console.debug("[store] " + m),
});

// Easy / Advanced is the browser's own; Dev = Advanced + system.devmode
export const mode = signal<Mode>(readPref<Mode>("gbsc.mode", "easy") === "advanced" ? "advanced" : "easy");
effect(() => writePref("gbsc.mode", mode.value));

const deskQuery = typeof matchMedia === "undefined" ? null : matchMedia("(min-width: 960px)");
export const desk = signal(!!deskQuery?.matches);
deskQuery?.addEventListener("change", (e) => { desk.value = e.matches; });

export const devmode = computed(() => Number(device.values.value["system.devmode"] ?? 0) === 1);
export const devOn = computed(() => mode.value === "advanced" && devmode.value);
export const offer = computed(() => offerVersion(String(device.info.value?.update?.offer?.version ?? ""), device.update.value.available, String(device.info.value?.fw ?? "")));
export const unsaved = computed(() => !!device.profiles.value?.active && !!device.profiles.value?.modified);
export const board = computed<"pro" | "standard">(() => (device.info.value?.board === "standard" ? "standard" : "pro"));
export const ready = computed(() => Object.keys(device.values.value).length > 0 && device.info.value !== null);

// The predicates' state (browser mode, settings, info, WiFi mode): rows read it to show, hide or grey themselves
export const pred = computed(() => stateFromStore({
  values: device.values.value, info: device.info.value, wifiMode: device.wifi.value?.mode ?? "", mode: mode.value,
  profileActive: !!device.profiles.value?.active,
}));

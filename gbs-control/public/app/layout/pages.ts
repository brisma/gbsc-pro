// The page description shared by the phone and PC renderers (data only, no JSX): pages, their sections (sidebar
// sub-items on PC, cards on the phone) and the groups and rows of each section. Names, icons and hints come from the
// web design and the TV menu.

import type { Page, Section, PageId, Row } from "./types.ts";
import { IN } from "../model/predicates.ts";
export type { PageId } from "./types.ts";
export type PageDef = Page;
export type SectionDef = Section;


// ---- Settings (rows in the TV menu's order; menu.json ids) ----
const m = (id: string) => `menu:${id}` as const;
const notDecoder = (s: { input: number }) => s.input !== IN.SV && s.input !== IN.AV;
const DECODER_NOTE: Row = { kind: "note", icon: "info", showIf: notDecoder,
  text: "Video standard, 2X and Decoder appear with S-Video or Composite, as on the TV." };

// Settings follow the signal chain, one stage per section: Input, Scaler, Picture, Output
const INPUT: Section = { id: "input", title: "Input", icon: "cable", hint: "The cable and its signal", groups: [
  { title: "Cable and sync", rows: [
    { kind: "picker", ref: m("input/source"), icon: "cable", look: "source" },
    { kind: "link", ref: m("input/auto-search"), icon: "travel_explore", phoneOnly: true },
    { kind: "inline", ref: m("input/wait-before-search"), icon: "hourglass_empty" },
    { kind: "inline", ref: m("input/video-standard~adv.format_sv"), icon: "public" },
    { kind: "inline", ref: m("input/video-standard~adv.format_av"), icon: "public" },
    { kind: "toggle", ref: m("input/2x"), icon: "filter_2" },
    { kind: "toggle", ref: m("input/2x-smooth"), icon: "blur_on" },
    { kind: "toggle", ref: m("input/adaptive-sync"), icon: "sync" },
    { kind: "toggle", ref: m("input/sync-cleanup"), icon: "cleaning_services" },
    { kind: "toggle", ref: m("input/vga-upscaling"), icon: "aspect_ratio" },
    { kind: "toggle", ref: m("input/75-ohm-sync"), icon: "electrical_services" },
    DECODER_NOTE,
  ] },
  { title: "Sampling", rows: [
    { kind: "toggle", ref: m("input/sampling/auto-gain"), icon: "brightness_auto" },
    { kind: "stepper", ref: m("input/sampling/gain"), icon: "exposure" },
    { kind: "stepper", ref: m("input/sampling/sog-level"), icon: "vertical_align_center" },
    { kind: "toggle", ref: m("input/sampling/boot-calibration"), icon: "rule" },
  ] },
  { title: "Decoder", rows: [
    { kind: "stepper", ref: m("input/decoder/brightness"), icon: "brightness_6" },
    { kind: "stepper", ref: m("input/decoder/contrast"), icon: "contrast" },
    { kind: "stepper", ref: m("input/decoder/saturation"), icon: "palette" },
    { kind: "stepper", ref: m("input/decoder/hue"), icon: "colorize" },
    { kind: "menu", ref: m("input/decoder/luma-comb~adv.comb_luma_ntsc"), icon: "grid_on" },
    { kind: "menu", ref: m("input/decoder/luma-comb~adv.comb_luma_pal"), icon: "grid_on" },
    { kind: "menu", ref: m("input/decoder/chroma-comb~adv.comb_chroma_ntsc"), icon: "grid_4x4" },
    { kind: "menu", ref: m("input/decoder/chroma-comb~adv.comb_chroma_pal"), icon: "grid_4x4" },
    { kind: "menu", ref: m("input/decoder/comb-split~adv.comb_ntsc"), icon: "call_split" },
    { kind: "menu", ref: m("input/decoder/comb-split~adv.comb_pal"), icon: "call_split" },
    { kind: "menu", ref: m("input/decoder/comb-taps~adv.comb_taps_ntsc"), icon: "view_week" },
    { kind: "menu", ref: m("input/decoder/comb-taps~adv.comb_taps_pal"), icon: "view_week" },
    { kind: "menu", ref: m("input/decoder/luma-filter~adv.filter_y"), icon: "tune" },
    { kind: "inline", ref: m("input/decoder/luma-filter-mode"), icon: "tune" },
    { kind: "menu", ref: m("input/decoder/luma-filter~adv.filter_wy"), icon: "tune" },
    { kind: "menu", ref: m("input/decoder/chroma-filter"), icon: "tune" },
    { kind: "toggle", ref: m("input/decoder/noise-reduction"), icon: "grain" },
    { kind: "toggle", ref: m("input/decoder/color-transients"), icon: "deblur" },
    { kind: "link", ref: m("input/decoder/adaptive-contrast"), icon: "wb_sunny" },
    { kind: "action", ref: m("input/decoder/reset-comb-filters"), icon: "settings_backup_restore" },
    { kind: "action", ref: m("input/decoder/reset-decoder"), icon: "restart_alt" },
  ] },
] };

const SCALER: Section = { id: "scaler", title: "Scaler", icon: "memory", hint: "Motion, size and position", groups: [
  { title: "Motion", rows: [
    { kind: "toggle", ref: m("output/smooth-scrolling"), icon: "sync_lock" },
    { kind: "inline", ref: m("output/lock-method"), icon: "swap_horiz" },
    { kind: "cards", ref: m("picture/deinterlace"), icon: "layers", look: "deint" },
  ] },
  { title: "Fit to the screen", rows: [
    { kind: "pad", ref: m("screen/position"), icon: "open_with" },
    { kind: "pad", ref: m("screen/size"), icon: "zoom_out_map" },
    { kind: "pad", ref: m("screen/edge-mask"), icon: "crop_free" },
    { kind: "pad", ref: m("screen/mask-position"), icon: "crop" },
    { kind: "toggle", ref: m("screen/pal-full-height"), icon: "height" },
    { kind: "action", ref: m("screen/reset-screen"), icon: "settings_backup_restore", head: true },
  ] },
  { title: "Clock and presets", rows: [
    { kind: "toggle", ref: m("output/external-clock"), icon: "schedule" },
    { kind: "toggle", ref: "setting:output.match_preset", icon: "fact_check", label: "Matched presets", advanced: true },
  ] },
] };

const PICTURE: Section = { id: "picture", title: "Picture", icon: "image", hint: "Look of the picture", groups: [
  { title: "Look", rows: [
    { kind: "cards", ref: m("picture/filter"), icon: "filter", look: "filter" },
    { kind: "cards", ref: m("picture/scanlines"), icon: "gradient", look: "scanlines" },
    { kind: "stepper", ref: m("picture/brightness"), icon: "brightness_6" },
    { kind: "stepper", ref: m("picture/contrast"), icon: "contrast" },
    { kind: "stepper", ref: m("picture/saturation"), icon: "palette" },
    { kind: "action", ref: m("picture/reset-picture"), icon: "settings_backup_restore", head: true },
  ] },
  { title: "Detail", rows: [
    { kind: "stepper", ref: m("picture/sharpness"), icon: "details" },
    { kind: "toggle", ref: m("picture/line-filter"), icon: "power_input" },
    { kind: "toggle", ref: m("picture/color-sharpness"), icon: "grain" },
  ] },
  { title: "Color", rows: [
    { kind: "stepper", ref: m("picture/red"), icon: "circle", tint: "red" },
    { kind: "stepper", ref: m("picture/green"), icon: "circle", tint: "green" },
    { kind: "stepper", ref: m("picture/blue"), icon: "circle", tint: "blue" },
    { kind: "action", ref: m("picture/reset-colors"), icon: "settings_backup_restore", head: true },
  ] },
] };

const OUTPUT: Section = { id: "output", title: "Output", icon: "tv", hint: "What goes to the TV", groups: [
  { title: "Resolution", rows: [
    { kind: "picker", ref: m("output/resolution"), icon: "hd", look: "resolution" },
    { kind: "toggle", ref: m("output/always-60hz"), icon: "60fps" },
  ] },
  { title: "Connection to the TV", rows: [
    { kind: "cards", ref: m("output/hdmi-range"), icon: "settings_input_hdmi", look: "hdmi" },
    { kind: "inline", ref: m("output/analog-out"), icon: "settings_input_component" },
  ] },
] };

// ---- System (menu.json ids). WiFi, Update and Backup draw themselves (pages/system/*.tsx, attached by the shell:
// this file stays data only); the rest are rows of the TV menu's System page ----
const sys = (id: string) => `menu:system/${id}` as const;
const WIFI: Section = { id: "wifi", title: "WiFi", icon: "wifi", hint: "Hotspot or your WiFi", bare: true };
const DEVICE: Section = { id: "device", title: "Device", icon: "tune", hint: "Sound, knob and board", groups: [
  { title: "Sound", rows: [
    { kind: "stepper", ref: sys("sound/volume"), icon: "volume_up" },
    { kind: "toggle", ref: sys("sound/mute"), icon: "volume_off" },
  ] },
  { title: "Knob", rows: [
    { kind: "menu", ref: sys("knob-direction"), icon: "adjust" },
    { kind: "inline", ref: sys("knob-steps"), icon: "adjust" },
  ] },
  { title: "Board", rows: [{ kind: "menu", ref: sys("board-type"), icon: "developer_board" }] },
] };
const TVMENU: Section = { id: "tvmenu", title: "TV and OLED menu", icon: "menu", hint: "The menu on the TV and on the OLED", groups: [
  { title: "Menu", rows: [
    { kind: "inline", ref: "menu:menu-mode", icon: "menu", label: "TV/OLED menu mode" },
    { kind: "menu", ref: sys("status-pop-up"), icon: "notifications" },
    { kind: "toggle", ref: sys("changes-on-tv"), icon: "live_tv" },
  ] },
  { title: "Look and place", rows: [
    { kind: "menu", ref: sys("osd-settings/theme"), icon: "palette" },
    { kind: "menu", ref: sys("osd-settings/menu-position"), icon: "picture_in_picture", look: "grid9" },
    { kind: "menu", ref: sys("osd-settings/pop-ups"), icon: "notifications", look: "grid9" },
    { kind: "menu", ref: sys("osd-settings/auto-close"), icon: "timer" },
  ] },
] };
const UPDATE: Section = { id: "update", title: "Update", icon: "system_update_alt", hint: "GBS-Control and ADV Controller", bare: true };
const BACKUP: Section = { id: "backup", title: "Backup", icon: "sd_card", hint: "Settings and profiles in one file", bare: true };
const RESET: Section = { id: "reset", title: "Restart and reset", icon: "restart_alt", hint: "Restart, or erase everything", groups: [
  { title: "Restart", rows: [
    { kind: "action", ref: sys("restart"), icon: "restart_alt", button: "Restart", bicon: "restart_alt", sub: "Restarts the GBSC PRO. Settings and profiles stay.",
      confirm: { title: "Restart?", body: "The device restarts; this page reconnects by itself.", ok: "Restart", icon: "restart_alt" } },
  ] },
  { title: "Erase", rows: [
    { kind: "action", ref: sys("factory-reset"), icon: "delete_forever", button: "Factory reset", bicon: "delete_forever", danger: true, sub: "Erases settings, profiles and WiFi, then restarts.",
      confirm: { title: "Factory reset?", body: "Erases settings and profiles and goes back to the hotspot. The device restarts.", ok: "Erase", icon: "delete_forever" } },
  ] },
] };

// The phone's way into the Developer page: the last System card, in Dev only
const DEVLINK: Section = { id: "devlink", title: "Developer tools", icon: "code", hint: "Scaler and device tools", to: "#/dev/sync", dev: true, phoneOnly: true };

// ---- Developer (menu.json Developer page). Toggles are the device's toggle actions with their /api/live readback;
// steppers without a readback show − and + only. Registers and the ADV Controller draw themselves (below) ----
const dv = (id: string) => `menu:developer/${id}` as const;
const SCALER_CHIP = "Scaler · TV5725";
const SYNC: Section = { id: "sync", title: "Sync", icon: "sync_problem", hint: "Watcher, polarity, HSync", group: SCALER_CHIP, groups: [
  { rows: [
    { kind: "inline", ref: dv("sync/watcher-profile"), icon: "manage_history" },
    { kind: "toggle", ref: dv("sync/sync-watcher"), icon: "devices_other" },
    { kind: "toggle", ref: dv("sync/sync-polarity"), icon: "invert_colors" },
    { kind: "stepper", ref: dv("sync/hsync-position"), icon: "keyboard_tab" },
    { kind: "action", ref: dv("sync/reset-sync"), icon: "sync_problem", button: "Reset" },
    { kind: "toggle", ref: dv("sync/if-auto-offset"), icon: "tune" },
  ] },
] };
const CLOCK: Section = { id: "clock", title: "Clock / ADC", icon: "straighten", hint: "HTotal, samples, SDRAM, capture shift", group: SCALER_CHIP, groups: [
  { rows: [
    { kind: "stepper", ref: dv("clock-adc/htotal"), icon: "straighten" },
    { kind: "action", ref: dv("clock-adc/resync-htotal"), icon: "sync_problem", button: "Resync" },
    { kind: "stepper", ref: dv("clock-adc/samples-per-line"), icon: "calculate" },
    { kind: "action", ref: dv("clock-adc/sdram-clock"), icon: "memory", button: "Next" },
    { kind: "action", ref: dv("clock-adc/oversampling"), icon: "insights", button: "Next" },
    { kind: "toggle", ref: dv("clock-adc/adc-filter"), icon: "filter_alt" },
    { kind: "action", ref: dv("clock-adc/snap-60-50hz"), icon: "speed", button: "Snap" },
  ] },
  { rows: [{ kind: "stepper", ref: dv("h-capture-shift"), icon: "keyboard_tab" }] },
] };
const DEBUG: Section = { id: "debug", title: "Debug", icon: "bug_report", hint: "Test views, freeze, OSD tests", group: SCALER_CHIP, groups: [
  { rows: [
    { kind: "toggle", ref: dv("debug/debug-view"), icon: "bug_report" },
    { kind: "toggle", ref: dv("debug/freeze-capture"), icon: "pause_circle" },
    { kind: "action", ref: dv("debug/reset-chip"), icon: "model_training", button: "Reset", danger: true,
      confirm: { title: "Reset the scaler chip?", body: "The TV5725 restarts and the picture goes away for a moment.", ok: "Reset", icon: "model_training" } },
    { kind: "toggle", ref: dv("debug/osd-test-screen"), icon: "grid_view" },
    { kind: "toggle", ref: dv("debug/osd-always-on"), icon: "visibility" },
    { kind: "action", ref: dv("debug/osd-rom-dump"), icon: "download", button: "Dump" },
  ] },
] };
// Draws itself (pages/dev/Registers.tsx, attached by pages/dev/renders.ts): every field of tv5725.h by name
const REGISTERS: Section = { id: "registers", title: "Registers", icon: "memory", hint: "Every TV5725 field by name", group: SCALER_CHIP, bare: true };
// ADV Controller tools (pages/dev/adv/, attached by pages/dev/renders.ts): options from the ADV Manager maps
const ADVG = "ADV Controller";
const DECODER: Section = { id: "decoder", title: "Decoder · ADV7280", icon: "input", hint: "Options from the ADV Manager register map", group: ADVG, cap: "adv", bare: true };
const ENCODER: Section = { id: "encoder", title: "Encoder · ADV7391", icon: "output", hint: "Options from the ADV Manager register map", group: ADVG, cap: "adv", bare: true };
const PRESETS: Section = { id: "presets", title: "Presets and macros", icon: "bookmarks", hint: "Stored on the device, ADV Manager .yaml", group: ADVG, cap: "adv", bare: true };
const MANUAL: Section = { id: "manual", title: "Manual and console", icon: "terminal", hint: "One register by its bits, the ADV Controller console", group: ADVG, cap: "adv", bare: true };
const LOG: Section = { id: "log", title: "Serial log", icon: "terminal", hint: "Print info, timings, files, IR codes", group: "Device", groups: [
  { rows: [
    { kind: "toggle", ref: dv("serial-log/print-info"), icon: "info" },
    { kind: "action", ref: dv("serial-log/print-timings"), icon: "alarm", button: "Print" },
    { kind: "action", ref: dv("serial-log/list-files"), icon: "list", button: "List" },
    { kind: "action", ref: "action:picture.color_info", icon: "palette", label: "Print colours", button: "Print" },   // old page: Filters › Info
    { kind: "action", ref: "action:dev.tv5725_dump", icon: "memory", label: "Print registers", button: "Print" },
    { kind: "toggle", ref: dv("serial-log/ir-codes-log"), icon: "settings_remote" },
    { kind: "toggle", ref: dv("serial-log/wifi-link-watch"), icon: "wifi" },
  ] },
] };
const ACCESS: Section = { id: "access", title: "Arduino IDE upload", icon: "upload", hint: "Flash from the Arduino IDE over WiFi", group: "Device", groups: [
  { rows: [
    { kind: "toggle", ref: dv("arduino-upload"), icon: "upload", label: "Enable OTA" },
    { kind: "secret", ref: dv("ota-password"), icon: "key", secret: "system.ota_password" },
  ] },
] };

export const PAGES: Page[] = [
  { id: "quick", title: "Quick", icon: "bolt", tab: true, sections: [] },
  { id: "settings", title: "Settings", icon: "tune", tab: true, sections: [INPUT, SCALER, PICTURE, OUTPUT] },
  { id: "profiles", title: "Profiles", icon: "sports_esports", tab: true, sections: [] },
  { id: "status", title: "Status", icon: "monitor_heart", tab: true, sections: [] },
  { id: "system", title: "System", icon: "settings", tab: true, sections: [WIFI, DEVICE, TVMENU, UPDATE, BACKUP, RESET, DEVLINK] },
  { id: "dev", title: "Developer", icon: "code", tab: false, sections: [SYNC, CLOCK, DEBUG, REGISTERS, DECODER, ENCODER, PRESETS, MANUAL, LOG, ACCESS] },
];

export const pageById = (id: string): PageDef | undefined => PAGES.find((p) => p.id === id);
// Sidebar group icons
export const GROUP_ICONS: Record<string, string> = { "Scaler · TV5725": "memory", "ADV Controller": "developer_board", "Device": "terminal" };   // @icons

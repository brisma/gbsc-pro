// The flasher's decisions: which flash is allowed, what needs a confirmation, what is offered after a flash
import { test } from "node:test";
import assert from "node:assert/strict";
import { flashEnabled, sizeOnlyWarning, isDowngrade, restoreProblem, preselect, advCheckOffered, keepLast, type DeviceRow, type FileRow } from "../src/state.ts";

const esp: DeviceRow = { port: "/dev/cu.wchusbserial1410", kind: "esp", label: "GBS-Control (ESP8266)", refuse: null, confirm: null };
const adv: DeviceRow = { port: "/dev/cu.usbmodem1", kind: "adv", label: "ADV Controller (bootloader)", refuse: null, confirm: null };
const consoleRow: DeviceRow = { port: "/dev/cu.usbmodem2", kind: "advConsole", label: "ADV Controller (application)", refuse: "hold the button", confirm: null };
const espFile: FileRow = { path: "/x/gbsc-pro-3.0.0.bin", name: "gbsc-pro-3.0.0.bin", kind: "esp", version: "3.0.0", bySizeOnly: false, advCrc: null, gzipped: false };
const advFile: FileRow = { path: "/x/adv-controller-3.0.0.bin", name: "adv-controller-3.0.0.bin", kind: "adv", version: "3.0.0", bySizeOnly: false, advCrc: 0x446fc362, gzipped: false };

test("flash_enabled_only_for_matching_kind", () => {
  assert.equal(flashEnabled(esp, espFile, "none"), true);
  assert.equal(flashEnabled(adv, advFile, "none"), true);
  assert.equal(flashEnabled(esp, advFile, "none"), false);
  assert.equal(flashEnabled(adv, espFile, "none"), false);
  assert.equal(flashEnabled(consoleRow, advFile, "none"), false);
  assert.equal(flashEnabled(esp, null, "none"), false);
  assert.equal(flashEnabled(esp, null, "wifi"), true);          // erase only
  assert.equal(flashEnabled(adv, null, "wifi"), false);
});

test("by_size_only_needs_confirmation", () => {
  assert.equal(sizeOnlyWarning(espFile), null);
  assert.match(sizeOnlyWarning({ ...espFile, bySizeOnly: true, version: null })!, /no GBSC header/);
});

test("downgrade_needs_confirmation", () => {
  assert.equal(isDowngrade("3.0.1", "3.0.0"), true);
  assert.equal(isDowngrade("3.0.0", "3.0.1-beta1"), false);
  assert.equal(isDowngrade("3.0.0", null), false);
});

test("restore_refuses_bad_backup", () => {
  assert.match(restoreProblem("hello")!, /.+/);
  assert.equal(restoreProblem("# gbsc-backup 1\n[file cfg/device.cfg]\nschema=1\n[file cfg/current.cfg]\nschema=1\n[profiles]\n"), null);
});

test("candidates_preselect", () => {
  const c = { esp: espFile, espWifi: espFile, adv: advFile, dir: "/x", translocated: false, fromZip: false };
  assert.equal(preselect(c, esp), espFile);
  assert.equal(preselect(c, adv), advFile);
  assert.equal(preselect(c, consoleRow), null);
  assert.equal(preselect({ ...c, esp: null }, esp), null);
});

test("adv_check_offered_after_usb_adv_flash", () => {
  assert.equal(advCheckOffered({ kind: "adv", ok: true }), true);
  assert.equal(advCheckOffered({ kind: "esp", ok: true }), false);
  assert.equal(advCheckOffered({ kind: "adv", ok: false }), false);
  assert.equal(advCheckOffered(null), false);
});

import { eraseConfirm, endText } from "../src/state.ts";

test("erase_only_warns_that_the_firmware_goes_too", () => {
  assert.match(eraseConfirm("user", false), /firmware/);
  assert.match(eraseConfirm("all", false), /firmware/);
  assert.doesNotMatch(eraseConfirm("wifi", false), /firmware/);
  assert.doesNotMatch(eraseConfirm("user", true), /firmware is erased/);
});

test("end_text_follows_what_happened", () => {
  assert.match(endText({ kind: "esp", ok: true, checked: true, seconds: 15, erasedOnly: false }), /verified/);
  assert.match(endText({ kind: "esp", ok: true, checked: null, seconds: 90, erasedOnly: false }), /not verified/);
  assert.match(endText({ kind: "esp", ok: true, checked: null, seconds: 20, erasedOnly: true }), /^Erased/);
  assert.match(endText({ kind: "adv", ok: true, checked: null, seconds: 10, erasedOnly: false }), /Unplug the USB-C/);
  assert.match(endText({ kind: "esp", ok: false, checked: null, seconds: 0, erasedOnly: false }), /did not finish/);
  // the reason in the box itself, not only in the log at the bottom
  const lost = endText({ kind: "esp", ok: false, checked: null, seconds: 0, erasedOnly: false, error: "The connection was lost." });
  assert.match(lost, /^The write did not finish: The connection was lost\.$/);
  assert.doesNotMatch(lost, /see the message below/);
});

import { advCheckFile, defaultDevice, flashQuestions } from "../src/state.ts";

test("adv_check_keeps_the_flashed_file", () => {
  // the file step may preselect another file (or none) once the USB-C cable is unplugged: the check stays on the
  // flashed one
  const last = { kind: "adv" as const, ok: true, file: advFile };
  assert.equal(advCheckFile(last), advFile);
  assert.equal(advCheckFile({ ...last, ok: false }), null);
  assert.equal(advCheckFile({ ...last, file: { ...advFile, advCrc: null } }), null);
  assert.equal(advCheckFile({ kind: "esp", ok: true, file: espFile }), null);
  assert.equal(advCheckFile(null), null);
});

const maybe: DeviceRow = { ...adv, port: "/dev/cu.usbmodemX", label: "USB device, maybe the ADV Controller (bootloader)", confirm: "may be another board (an Arduino)" };

test("an_unconfirmed_port_is_asked_about_and_not_preselected", () => {
  assert.equal(defaultDevice([maybe, adv], null), adv.port);
  assert.equal(defaultDevice([maybe], null), maybe.port);           // shown, but flashing it asks first
  assert.equal(defaultDevice([maybe, adv], maybe.port), maybe.port); // the user's choice stays
  assert.equal(defaultDevice([], null), null);
  assert.equal(defaultDevice([consoleRow, esp], null), esp.port);
  const asks = flashQuestions(maybe, advFile, "none");
  assert.equal(asks.length, 1);
  assert.match(asks[0].body, /Arduino/);
  assert.equal(flashQuestions(adv, advFile, "none").length, 0);
  assert.equal(flashQuestions(esp, { ...espFile, bySizeOnly: true }, "user").length, 2);
});

test("another_flashable_device_drops_the_last_result", () => {
  const espDone = { kind: "esp" as const, ok: true, port: esp.port, ports: [esp.port] };
  assert.equal(keepLast(espDone, esp, false), true);            // the GBSC restarts on the same port
  assert.equal(keepLast(espDone, null, false), true);           // unplugged: the end step stays
  assert.equal(keepLast(espDone, adv, false), false);           // the ADV bootloader is plugged next: a new flash
  const advDone = { kind: "adv" as const, ok: true, port: adv.port, file: advFile, ports: [adv.port, esp.port] };
  assert.equal(keepLast(advDone, consoleRow, false), true);     // the ADV Controller restarts as its application: WiFi check
  assert.equal(keepLast(null, esp, false), true);
  // the GBSC's own USB cable stays plugged; unplugging the USB-C moves the selection to it by itself, and the ADV
  // result with its WiFi check must stay
  assert.equal(keepLast(advDone, esp, false), true);
  assert.equal(keepLast(advDone, esp, true), false);            // the user picked the ESP8266: a new flash
});

import { cleanHost } from "../src/state.ts";

test("erase_questions_only_for_the_esp8266", () => {
  // the erase choice stays from an ESP8266 job; the ADV Controller has no erase
  assert.equal(flashQuestions(adv, advFile, "all").length, 0);
  assert.equal(flashQuestions(esp, espFile, "all").length, 1);
});

test("an_erase_without_a_firmware_says_so", () => {
  assert.match(endText({ kind: "esp", ok: true, checked: null, seconds: 20, erasedOnly: true, erase: "wifi" }), /restarts/);
  const gone = endText({ kind: "esp", ok: true, checked: null, seconds: 20, erasedOnly: true, erase: "all" });
  assert.doesNotMatch(gone, /restarts/);
  assert.match(gone, /firmware/);
});

test("a_pasted_address_is_cleaned", () => {
  assert.equal(cleanHost(" http://192.168.1.50/ "), "192.168.1.50");
  assert.equal(cleanHost("https://gbscontrol.local/api/info"), "gbscontrol.local");
  assert.equal(cleanHost("192.168.1.50:8080"), "192.168.1.50:8080");
  assert.equal(cleanHost("192.168.1.50"), "192.168.1.50");
});

import { inFolder, notFoundHint } from "../src/state.ts";

test("file_dialogs_start_in_the_flasher_folder", () => {
  // without a start folder Windows opened the dialog in C:\Windows\System32 (the process folder)
  assert.equal(inFolder("C:\\Users\\Me\\gbsc-pro-3.0.0", "backup.txt"), "C:\\Users\\Me\\gbsc-pro-3.0.0\\backup.txt");
  assert.equal(inFolder("C:\\Users\\Me\\gbsc-pro-3.0.0\\", "backup.txt"), "C:\\Users\\Me\\gbsc-pro-3.0.0\\backup.txt");
  assert.equal(inFolder("/Users/me/gbsc-pro-3.0.0", "backup.txt"), "/Users/me/gbsc-pro-3.0.0/backup.txt");
  assert.equal(inFolder("", "backup.txt"), "backup.txt");
});

test("not_found_names_the_windows_firewall", () => {
  // the first search asks for the Firewall permission; "Cancel" there blocks every answer
  assert.match(notFoundHint(true), /Firewall/);
  assert.doesNotMatch(notFoundHint(false), /Firewall/);
  assert.match(notFoundHint(false), /type its IP address/);
});

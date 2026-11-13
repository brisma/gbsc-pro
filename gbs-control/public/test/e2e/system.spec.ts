// System page: WiFi (details, Extra hosts, scan and connect), Update (offer, upload with the image version, low memory,
// failure), Backup (restore check and progress), Restart and reset, the ADV Controller row
import { test, expect, expectConsoleError, allowNavigationAborts } from "./fixtures.ts";
import type { Page } from "@playwright/test";
import type { SimHandle } from "./fixtures.ts";

const isDesk = (page: Page) => (page.viewportSize()?.width ?? 0) >= 960;
async function open(page: Page, sec: string, mode: "easy" | "advanced" = "easy"): Promise<void> {
  await page.addInitScript((m) => localStorage.setItem("gbsc.mode", JSON.stringify(m)), mode);
  await page.goto(`/#/system/${sec}`);
  await expect(page.locator(".ui[data-ready]")).toBeVisible();
}
const section = (page: Page, id: string) => page.locator(`[data-section="${id}"]`);
const posts = async (sim: SimHandle, path: string) =>
  (await sim.requests()).requests.filter((r) => r.method === "POST" && r.path === path).map((r) => r.body);
const installed = (page: Page) => section(page, "update").locator('[data-row$="update/gbs-control"] .val');   // the version that runs
const image = (version: string) => Buffer.concat([Buffer.alloc(3000, 0xe9), Buffer.from(`GBSCVER=${version};`), Buffer.alloc(3000, 1)]);

// The device restarts in this test: polls in flight fail (network errors, not HTTP ones), WebKit logs the WebSocket.
// Each engine words the dropped connection its own way: Chromium, WebKit on macOS, on Linux, on Windows.
function restarts(page: Page): void {
  allowNavigationAborts(page);
  expectConsoleError(page, /WebSocket|Failed to load resource: (net::ERR_|The network connection was lost|Connection terminated unexpectedly|Server returned nothing)/);
}

async function chooseFile(page: Page, button: RegExp, name: string, data: Buffer): Promise<void> {
  const chooser = page.waitForEvent("filechooser");
  await page.getByRole("button", { name: button }).click();
  await (await chooser).setFiles({ name, mimeType: "application/octet-stream", buffer: data });
}

// ---- WiFi ----
test("wifi shows hostname and the AP password only in AP mode", async ({ page, sim }) => {
  restarts(page);
  await open(page, "wifi");
  const s = section(page, "wifi");
  await expect(s).toContainText("gbscontrol.local");
  await expect(s).toContainText("Home");
  await expect(s.getByText("Password", { exact: true })).toHaveCount(0);
  sim.sim!.state.wifi.mode = "AP";
  await page.reload();
  await expect(page.locator(".ui[data-ready]")).toBeVisible();
  await expect(s).toContainText("gbscontrol");
  await expect(s.getByText("Password", { exact: true })).toBeVisible();
  await expect(s).not.toContainText("qqqqqqqq");                       // hidden until asked
  await s.getByRole("button", { name: "Show the password" }).click();
  await expect(s).toContainText("qqqqqqqq");
});

test("extra hosts invalid name shows the rule", async ({ page, sim }) => {
  await open(page, "wifi", "advanced");
  await section(page, "wifi").getByRole("button", { name: /^Extra hosts/ }).click();
  const box = page.getByRole("dialog", { name: "Extra hosts" });
  await box.getByRole("textbox").fill("tv_box.lan");
  await expect(box).toContainText("Names use letters, digits, dots and dashes only");
  await expect(box.getByRole("button", { name: "Save" })).toBeDisabled();
  await box.getByRole("textbox").fill("tv-box.lan, cab.fritz.box");
  await expect(box.getByRole("button", { name: "Save" })).toBeEnabled();
  await box.getByRole("button", { name: "Save" }).click();
  await expect.poll(() => posts(sim, "/api/settings")).toEqual(["system.extra_hosts=tv-box.lan%2C+cab.fritz.box"]);
  await expect(section(page, "wifi")).toContainText("tv-box.lan, cab.fritz.box");
});

test("extra hosts dropping this page's name explains in_use", async ({ page, sim }) => {
  await open(page, "wifi", "advanced");
  await sim.post("/__sim/fault", { name: "hostsInUse" });
  expectConsoleError(page, /409/);
  await section(page, "wifi").getByRole("button", { name: /^Extra hosts/ }).click();
  const box = page.getByRole("dialog", { name: "Extra hosts" });
  await box.getByRole("textbox").fill("other.lan");
  await box.getByRole("button", { name: "Save" }).click();
  await expect(box).toContainText("this page was opened with one of the names you removed");
  await expect(box).toBeVisible();
});

test("scan then connect to an open network", async ({ page, sim }) => {
  await open(page, "wifi");
  await section(page, "wifi").getByRole("button", { name: "Scan" }).click();
  await page.getByRole("button", { name: "Join Cafe" }).click();
  const box = page.getByRole("dialog", { name: "Cafe" });
  await expect(box).toContainText("open network");
  await expect(box.getByLabel("Password")).toHaveCount(0);
  await box.getByRole("button", { name: "Connect" }).click();
  await expect.poll(() => posts(sim, "/api/wifi/connect")).toEqual(["ssid=Cafe"]);
  await expect(page.getByRole("dialog")).toContainText("gbscontrol.local");   // where to find the page afterwards
});

test("connect to a secured network", async ({ page, sim }) => {
  await open(page, "wifi");
  await section(page, "wifi").getByRole("button", { name: "Scan" }).click();
  await page.getByRole("button", { name: "Join Neighbour 5G" }).click();
  const box = page.getByRole("dialog", { name: "Neighbour 5G" });
  await expect(box.getByRole("button", { name: "Connect" })).toBeDisabled();
  await box.getByLabel("Password").fill("pass phrase!");
  await box.getByRole("button", { name: "Connect" }).click();
  await expect.poll(() => posts(sim, "/api/wifi/connect")).toEqual(["ssid=Neighbour+5G&password=pass+phrase%21"]);
});

test("reset to hotspot asks first", async ({ page, sim }) => {
  restarts(page);
  await open(page, "wifi");
  await section(page, "wifi").locator('[data-row$="reset-to-hotspot"]').getByRole("button").click();
  await page.getByRole("dialog").getByRole("button", { name: "Cancel" }).click();
  expect(await posts(sim, "/api/wifi/reset")).toEqual([]);
  await section(page, "wifi").locator('[data-row$="reset-to-hotspot"]').getByRole("button").click();
  await page.getByRole("dialog").getByRole("button", { name: "Reset" }).click();
  await expect.poll(() => posts(sim, "/api/wifi/reset")).toEqual([""]);
});

// ---- Update ----
test("install the offer with progress then done", async ({ page, sim }) => {
  restarts(page);
  await sim.post("/__sim/scenario", { name: "update-offer" });
  await open(page, "update");
  const s = section(page, "update");
  await expect(s.locator('[data-row$="update/install"]')).toContainText("GBS-Control 3.0.1");   // the offer, in the Available group
  await page.evaluate(() => { (window as unknown as { beforeInstall?: number }).beforeInstall = 1; });
  await s.getByRole("button", { name: "Install", exact: true }).click();
  await page.getByRole("dialog").getByRole("button", { name: "Install" }).click();
  const bar = s.getByRole("progressbar");
  await expect(bar).toBeVisible();
  const handle = await bar.elementHandle();
  await expect(bar).toHaveAccessibleName("Installing 3.0.1");
  await expect(s.locator(".pbar b")).toHaveText(/^\d+%$/);
  await expect.poll(async () => Number(await bar.getAttribute("aria-valuenow"))).toBeGreaterThan(40);
  expect(await handle!.evaluate((el) => el.isConnected), "the progress bar was re-created").toBe(true);
  await expect.poll(() => posts(sim, "/api/actions/system.install_update")).toEqual([""]);
  // the page reloads into the new firmware's page once the device is back (the old bundle must not stay in
  // use); while it navigates, evaluate throws
  await expect.poll(async () => {
    try { return await page.evaluate(() => (window as unknown as { beforeInstall?: number }).beforeInstall ?? "reloaded"); } catch { return "navigating"; }
  }, { timeout: 15000 }).toBe("reloaded");
  await expect(installed(page)).toContainText("3.0.1", { timeout: 10000 });
});

test("upload an older image asks first", async ({ page, sim }) => {
  restarts(page);
  await open(page, "update");
  await chooseFile(page, /^Choose file/, "gbsc-pro-2.9.0.bin", image("2.9.0"));
  const box = page.getByRole("dialog", { name: "Install an older version?" });
  await expect(box).toContainText("2.9.0");
  await expect(box).toContainText("3.0.0-dev0");
  await box.getByRole("button", { name: "Cancel" }).click();
  expect(await posts(sim, "/api/system/update")).toEqual([]);
  await sim.post("/__sim/fault", { name: "slowUpload" });   // time to see the bar
  await chooseFile(page, /^Choose file/, "gbsc-pro-2.9.0.bin", image("2.9.0"));
  await page.getByRole("dialog").getByRole("button", { name: "Install" }).click();
  await expect(section(page, "update").getByRole("progressbar")).toBeVisible();
  await expect.poll(async () => (await posts(sim, "/api/system/update")).length).toBe(1);
  await expect(installed(page)).toContainText("3.0.1-dev0", { timeout: 10000 });   // the simulator's next image
});

test("upload refused with low memory is explained", async ({ page, sim }) => {
  await open(page, "update");
  await sim.post("/__sim/scenario", { name: "low-memory" });
  expectConsoleError(page, /503/);
  await chooseFile(page, /^Choose file/, "gbsc-pro-3.0.1.bin", image("3.0.1"));
  await page.getByRole("dialog").getByRole("button", { name: "Install" }).click();
  await expect(section(page, "update")).toContainText("not enough free memory");
});

test("upload failure shows the reason", async ({ page, sim }) => {
  await open(page, "update");
  await sim.post("/__sim/fault", { name: "uploadFail" });
  await chooseFile(page, /^Choose file/, "gbsc-pro-3.0.1.bin", image("3.0.1"));
  await page.getByRole("dialog").getByRole("button", { name: "Install" }).click();
  await expect(section(page, "update")).toContainText("the image did not verify");
});

test("ADV Controller update only when offered on Pro", async ({ page, sim }) => {
  restarts(page);
  await open(page, "update");
  const row = section(page, "update").locator('[data-row$="adv-controller"]');
  await expect(row).toContainText("3.0.0");
  await expect(row.getByRole("button", { name: "Update" })).toHaveCount(0);
  await sim.post("/__sim/scenario", { name: "adv-update-offer" });
  await page.reload();
  await expect(page.locator(".ui[data-ready]")).toBeVisible();
  await row.getByRole("button", { name: "Update" }).click();
  await page.getByRole("dialog").getByRole("button", { name: "Update" }).click();
  await expect.poll(() => posts(sim, "/api/actions/system.update_adv_controller")).toEqual([""]);
  await expect(row).toContainText(/updated/i, { timeout: 8000 });
  await sim.post("/__sim/scenario", { name: "standard-rgbs" });
  await page.reload();
  await expect(page.locator(".ui[data-ready]")).toBeVisible();
  await expect(section(page, "update").locator('[data-row$="adv-controller"]')).toHaveCount(0);
});

test("another channel offers its build to install", async ({ page, sim }) => {
  await open(page, "update", "advanced");
  const s = section(page, "update");
  await expect(s.getByRole("radiogroup", { name: "Channel" }).getByRole("radio")).toHaveText(["Stable", "Beta"]);   // two channels
  await s.getByRole("radio", { name: "Stable" }).click();                 // the simulated build follows Beta
  await expect.poll(() => posts(sim, "/api/actions/system.check_channel")).toEqual(["data=stable"]);
  const offerRow = s.locator('[data-row$="other-channels/offer"]');     // the Stable build, offered with Install
  await expect(offerRow).toContainText("3.0.0");
  await expect(offerRow.getByRole("button", { name: "Install" })).toBeVisible();
});

// ---- Backup ----
test("restore asks then reports", async ({ page, sim }) => {
  restarts(page);
  await open(page, "backup");
  await chooseFile(page, /^Choose a file to restore/, "notes.txt", Buffer.from("hello\n"));
  await expect(page.getByRole("dialog")).toContainText("Not a GBSC backup");
  await page.getByRole("dialog").getByRole("button", { name: "Close" }).click();
  expect(await posts(sim, "/api/system/restore")).toEqual([]);
  const backup = await (await page.request.get("/api/system/backup")).text();   // Node fetch may reuse a socket the device closed
  await chooseFile(page, /^Choose a file to restore/, "gbsc-backup.txt", Buffer.from(backup));
  const box = page.getByRole("dialog", { name: "Restore this backup?" });
  await expect(box).toContainText("restarts");
  await box.getByRole("button", { name: "Restore" }).click();
  await expect.poll(async () => (await posts(sim, "/api/system/restore")).length).toBe(1);
  await expect(section(page, "backup")).toContainText("Restored");
});

// The backup names the profile that was active: a restore brings it back, whatever was loaded since
test("a restore brings back the backup's active profile", async ({ page, sim }) => {
  restarts(page);
  await sim.post("/__sim/scenario", { name: "unsaved-profile" });   // Saturn is active
  const backup = await (await page.request.get("/api/system/backup")).text();
  expect((await page.request.post("/api/profiles/defaults", { headers: { "X-GBSC": "1" } })).ok()).toBe(true);
  await open(page, "backup");
  const chip = page.getByRole("link", { name: /^Profile: / });
  await expect(chip).toHaveAccessibleName("Profile: Defaults");
  await chooseFile(page, /^Choose a file to restore/, "gbsc-backup.txt", Buffer.from(backup));
  await page.getByRole("dialog", { name: "Restore this backup?" }).getByRole("button", { name: "Restore" }).click();
  await expect(section(page, "backup")).toContainText("Restored");
  await expect(chip).toHaveAccessibleName("Profile: Saturn · NTSC-J");
});

test("download backup saves the device's file", async ({ page }) => {
  await open(page, "backup");
  const dl = page.waitForEvent("download");
  await page.getByRole("button", { name: /^Download backup/ }).click();
  expect((await dl).suggestedFilename()).toMatch(/^gbsc-backup-\d{4}-\d{2}-\d{2}\.txt$/);
});

test("oversized backup is flagged", async ({ page, sim }) => {
  await sim.post("/__sim/fault", { name: "bigBackup" });   // more than the 640 KB a restore takes
  await open(page, "backup");
  const dl = page.waitForEvent("download");
  await page.getByRole("button", { name: /^Download backup/ }).click();
  await dl;
  await expect(page.getByText(/over 640 KB/)).toBeVisible();
});

// ---- Restart and reset ----
test("factory reset asks with Erase", async ({ page, sim }) => {
  restarts(page);
  await open(page, "reset");
  await section(page, "reset").locator('[data-row$="factory-reset"]').getByRole("button").click();
  const box = page.getByRole("dialog", { name: "Factory reset?" });
  await expect(box).toContainText("profiles");
  await box.getByRole("button", { name: "Erase" }).click();
  await expect.poll(() => posts(sim, "/api/actions/system.factory_reset")).toEqual([""]);
});

test("restart is there in Easy too and asks first", async ({ page, sim }) => {
  restarts(page);
  await open(page, "reset");
  const row = section(page, "reset").locator('[data-row$="system/restart"]');
  await row.getByRole("button", { name: "Restart" }).click();
  await page.getByRole("dialog", { name: "Restart?" }).getByRole("button", { name: "Cancel" }).click();
  expect(await posts(sim, "/api/actions/system.restart")).toEqual([]);
});

test("system sections in order, standard board shows Changes on TV", async ({ page, sim }) => {
  allowNavigationAborts(page);   // a second goto and a reload
  await open(page, "wifi");
  // PC: under System in the sidebar; phone: the tabs under the header, with short names where six do not fit
  const titles = isDesk(page)
    ? await page.locator("a.subl").allInnerTexts()
    : await page.getByRole("navigation", { name: "System sections" }).getByRole("link").allInnerTexts();
  expect(titles.map((t) => t.trim())).toEqual(isDesk(page) ? ["WiFi", "Device", "TV and OLED menu", "Update", "Backup", "Restart and reset"]
    : ["WiFi", "Device", "TV menu", "Update", "Backup", "Reset"]);
  await page.goto("/#/system/tvmenu");
  await expect(section(page, "tvmenu")).toContainText("Status pop-up");
  await expect(section(page, "tvmenu")).not.toContainText("Changes on TV");
  await sim.post("/__sim/scenario", { name: "standard-rgbs" });
  await page.reload();
  await expect(page.locator(".ui[data-ready]")).toBeVisible();
  await expect(section(page, "tvmenu")).toContainText("Changes on TV");
  await expect(section(page, "tvmenu")).not.toContainText("Status pop-up");
});

test("check now reports the result; Advanced shows the last result and Auto check", async ({ page, sim }) => {
  await open(page, "update", "advanced");
  const s = section(page, "update");
  await expect(s.locator('[data-row$="last-result"]')).toContainText("Not checked yet");
  await s.getByRole("button", { name: "Check now" }).click();   // the button of the card
  await expect.poll(() => posts(sim, "/api/actions/system.check_update")).toEqual([""]);
  await expect(page.getByText("Up to date").first()).toBeVisible();
  await expect(s.locator('[data-row$="last-result"]')).toContainText("Up to date");
  await s.locator('[data-row$="auto-check"]').getByRole("radio", { name: "Off" }).click();   // Daily is the default
  await expect.poll(() => posts(sim, "/api/settings")).toEqual(["system.auto_update_check=0"]);
});

// The same version again (every local build is -dev0) must not leave the job hanging
test("uploading the same version ends after the restart", async ({ page, sim }) => {
  restarts(page);
  sim.sim!.nextFw = "3.0.0-dev0";
  await open(page, "update");
  await chooseFile(page, /^Choose file/, "gbsc-pro-3.0.0-dev0.bin", image("3.0.0-dev0"));
  await page.getByRole("dialog").getByRole("button", { name: "Install" }).click();
  await expect.poll(async () => (await posts(sim, "/api/system/update")).length).toBe(1);
  await expect(section(page, "update")).toContainText("Up to date", { timeout: 15000 });
  await expect(section(page, "update").getByRole("button", { name: /^Choose file/ })).toBeEnabled();
});

test("an upload the device refuses with 400 shows its reason", async ({ page, sim }) => {
  await open(page, "update");
  await sim.post("/__sim/fault", { name: "uploadBadRequest" });
  expectConsoleError(page, /400/);
  await chooseFile(page, /^Choose file/, "gbsc-pro-3.0.1.bin", image("3.0.1"));
  await page.getByRole("dialog").getByRole("button", { name: "Install" }).click();
  await expect(section(page, "update")).toContainText("the file is not a firmware image");
});

test("Optimize WiFi only in Advanced, on by default", async ({ page, sim }) => {
  const row = () => section(page, "wifi").locator('[data-row$=":menu:system/wifi/optimize-wifi"]').first();
  await open(page, "wifi");
  await expect(row()).toHaveCount(0);
  await page.getByRole("radiogroup", { name: "Mode" }).getByRole("radio", { name: /^Adv/ }).click();
  const sw = row().getByRole("switch");
  await expect(sw).toHaveAttribute("aria-checked", "true");
  await sw.click();
  await expect.poll(() => posts(sim, "/api/settings")).toEqual(["system.wifi_optimize=0"]);
});

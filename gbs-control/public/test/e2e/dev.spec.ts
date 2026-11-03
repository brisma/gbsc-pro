// Developer page: only in Dev (PC sidebar entry, phone System card "Developer tools" last), the scaler and device tools
// from the menu's Developer page, live toggles, the write-only OTA password. Every row's action or setting is checked
// by every-row.spec.ts (generated from layout/pages.ts).
import { test, expect } from "./fixtures.ts";
import type { Page } from "@playwright/test";
import type { SimHandle } from "./fixtures.ts";
import { ITEMS } from "./rows.ts";

const isDesk = (page: Page) => (page.viewportSize()?.width ?? 0) >= 960;
async function open(page: Page, sim: SimHandle, hash: string, dev = true): Promise<void> {
  if (dev) {   // the device applies a TV change in its loop: the page must load with Dev already on
    await sim.post("/__sim/tv", { key: "system.devmode", value: "1" });
    await expect.poll(async () => (await (await page.request.get("/api/settings")).json())["system.devmode"]).toBe(1);
  }
  await page.addInitScript(() => localStorage.setItem("gbsc.mode", '"advanced"'));
  await page.goto("/" + hash);
  await expect(page.locator(".ui[data-ready]")).toBeVisible();
}
const posts = async (sim: SimHandle, path: string) =>
  (await sim.requests()).requests.filter((r) => r.method === "POST" && r.path === path).map((r) => r.body);

// The phone's way into the Developer page: the last tab of System, in Dev only
const devTab = (page: Page) => page.getByRole("navigation", { name: "System sections" }).getByRole("link", { name: "Developer" });

test("developer hidden outside Dev", async ({ page, sim }) => {
  await open(page, sim, "#/dev/sync", false);
  await expect(page).toHaveURL(/#\/system/);                         // a bookmark lands on System
  if (isDesk(page)) await expect(page.locator(".side").getByRole("link", { name: "Developer" })).toHaveCount(0);
  else await expect(devTab(page)).toHaveCount(0);
});

test("opens on Sync", async ({ page, sim }) => {
  await open(page, sim, "#/system");
  if (isDesk(page)) await page.locator(".side").getByRole("link", { name: "Developer" }).click();
  else {
    await expect(page.getByRole("navigation", { name: "System sections" }).getByRole("link").last()).toHaveText("Developer");   // the last System tab
    await devTab(page).click();
  }
  await expect(page).toHaveURL(/#\/dev\/sync$/);
  await expect(page.locator('[data-section="sync"]')).toContainText("Sync watcher");
});

test("phone Developer header goes back to System", async ({ page, sim }) => {
  await open(page, sim, "#/dev/sync");
  const back = page.locator(".hd").getByRole("button", { name: "Back to System" });
  if (isDesk(page)) { await expect(back).toHaveCount(0); return; }   // the PC has the sidebar
  await back.click();
  await expect(page).toHaveURL(/#\/system/);
  await expect(devTab(page)).toBeVisible();
});

test("live toggles show the device state", async ({ page, sim }) => {
  await open(page, sim, "#/dev/debug");
  const sw = page.locator('[data-row$="developer/debug/debug-view"]').getByRole("switch");
  await expect(sw).toHaveAttribute("aria-checked", "false");
  await sim.post("/__sim/live", { name: "debug_view", value: 1 });   // changed on the TV
  await expect(sw).toHaveAttribute("aria-checked", "true");
  await sw.click();
  await expect.poll(() => posts(sim, "/api/actions/dev.debug_view")).toEqual([""]);
  await expect(sw).toHaveAttribute("aria-checked", "false");          // the readback after the action
});

test("steppers without a readback show only − and +, with one the value", async ({ page, sim }) => {
  await open(page, sim, "#/dev/sync");
  const row = page.locator('[data-row$="developer/sync/hsync-position"]');
  await expect(row.getByRole("button", { name: "Decrease HSync position" })).toBeVisible();
  await expect(row.locator(".stp b")).toHaveCount(0);
  await page.goto("/#/dev/clock");
  await expect(page.locator('[data-row$="developer/clock-adc/htotal"] .stp b')).toHaveText("2035");   // with a readback: the value
});

test("OTA password is write-only", async ({ page, sim }) => {
  await open(page, sim, "#/dev/access");
  const row = page.locator('[data-row$="developer/ota-password"]');
  await expect(row).toContainText("Not set");
  await row.getByRole("button", { name: /^Change OTA password/ }).click();
  const box = page.getByRole("dialog", { name: "OTA password" });
  await box.getByLabel("New password").fill("s3cret-pw");
  await box.getByRole("button", { name: "Save" }).click();
  await expect.poll(() => posts(sim, "/api/settings")).toEqual(["system.ota_password=s3cret-pw"]);
  await expect(row).toContainText("Set");
  await expect(page.locator("main")).not.toContainText("s3cret-pw");
  // the device (and the simulator) applies settings in its loop: the masked value shows once it ran
  await expect.poll(async () => (await (await page.request.get("/api/settings")).json())["system.ota_password"]).toBe("***");
});

test("every-row covers the developer tools", () => {
  const dev = ITEMS.filter((i) => i.page === "dev").map((i) => i.title);
  for (const a of ["dev.debug_view", "dev.reset_chip", "dev.htotal_up", "dev.hs_right", "dev.enable_ota", "dev.sdram_cycle", "dev.mem_right"])
    expect(dev, a).toContain(`every-row action:${a}`);
  expect(dev).toContain("every-row setting:dev.syncwatcher advanced");
});

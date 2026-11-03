// Row components on the kitchen sink page /__sim/components
import { test, expect } from "./fixtures.ts";
import type { Page, Locator } from "@playwright/test";
import type { SimHandle } from "./fixtures.ts";

const isDesk = (page: Page) => (page.viewportSize()?.width ?? 0) >= 960;
const posts = async (sim: SimHandle, path: string) =>
  (await sim.requests()).requests.filter((r) => r.method === "POST" && r.path === path).map((r) => r.body);
const row = (page: Page, ref: string): Locator => page.locator(`[data-row$=":${ref}"]`).first();
async function open(page: Page, q = ""): Promise<void> {
  await page.goto("/__sim/components" + q);
  await expect(page.locator(".ui[data-ready]")).toBeVisible();
}

test("stepper holds to repeat", async ({ page, sim }) => {
  await open(page);
  const plus = row(page, "menu:picture/brightness").getByRole("button", { name: "Increase Brightness" });
  await plus.hover();
  await page.mouse.down();
  await page.waitForTimeout(1100);
  await page.mouse.up();
  await expect.poll(async () => (await posts(sim, "/api/settings")).length).toBeGreaterThanOrEqual(3);
  const bodies = await posts(sim, "/api/settings");
  const vals = bodies.map((b) => Number(new URLSearchParams(b).get("picture.brightness")));
  expect(vals.every((v, i) => i === 0 || v > vals[i - 1])).toBe(true);   // each step from the value just shown
});

test("stepper value offers reset to default", async ({ page, sim }) => {
  await sim.post("/__sim/tv", { key: "picture.brightness", value: "70" });
  await open(page);
  const r = row(page, "menu:picture/brightness");
  await expect(r.getByRole("button", { name: /^Brightness 70/ })).toBeVisible();
  await r.getByRole("button", { name: /^Brightness 70/ }).click();
  await page.getByRole("menuitem", { name: /^Reset to/ }).click();
  await expect.poll(() => posts(sim, "/api/settings")).toEqual([expect.stringMatching(/^picture\.brightness=\d+$/)]);
  expect((await posts(sim, "/api/settings"))[0]).not.toBe("picture.brightness=70");
});

test("stepper without readback shows only the arrows", async ({ page, sim }) => {
  await sim.post("/__sim/tv", { key: "system.devmode", value: "1" });
  await open(page, "?mode=advanced");
  const r = row(page, "menu:developer/sync/hsync-position");
  await expect(r).toBeVisible();
  await expect(r.locator(".stp button")).toHaveCount(2);
  await expect(r.locator(".stp b")).toHaveCount(0);
  await r.getByRole("button", { name: "Increase HSync position" }).click();
  await expect.poll(() => posts(sim, "/api/actions/dev.hs_right")).toHaveLength(1);
  const gain = row(page, "menu:input/sampling/gain");   // with a readback: the live value between the arrows
  await expect(gain.locator(".stp b")).toHaveText("132");
});

test("long option lists use the dropdown", async ({ page, sim }) => {
  await open(page, "?mode=advanced");   // OSD settings is an Advanced page on the TV
  const pos = row(page, "menu:system/osd-settings/menu-position");
  await expect(pos.getByRole("radio")).toHaveCount(0);
  await pos.getByRole("button", { name: /^Menu position:/ }).click();
  const list = page.getByRole("listbox", { name: "Menu position" });
  await expect(list).toBeVisible();
  const opts = list.getByRole("option");
  await opts.nth(1).click();
  await expect(list).toHaveCount(0);
  await expect.poll(async () => (await posts(sim, "/api/settings")).length).toBe(1);
  expect((await posts(sim, "/api/settings"))[0]).toMatch(/^osd\.position=\d+$/);
  const hdmi = row(page, "menu:output/hdmi-range");   // short lists: segments
  await hdmi.getByRole("radio", { name: "Full" }).click();
  await expect.poll(async () => (await posts(sim, "/api/settings")).at(-1)).toBe("output.hdmi_range=0");   // Full = 0 (OPT_HDMI)
});

test("look choices are segments, always open, one click each", async ({ page, sim }) => {
  await open(page);
  await page.getByRole("radiogroup", { name: "Filter" }).getByRole("radio", { name: /Sharp/ }).click();
  await expect.poll(() => posts(sim, "/api/settings")).toEqual(["picture.sharpness=31&picture.line_filter=1"]);
  const group = page.getByRole("radiogroup", { name: "Scanlines" });
  await expect(group).toBeVisible();
  await group.getByRole("radio", { name: /Medium/ }).click();
  await expect.poll(async () => (await posts(sim, "/api/settings")).at(-1)).toBe("picture.scanlines=1&picture.scanline_strength=48");
  await expect(group).toBeVisible();
});

test("picker strip on PC", async ({ page, sim }) => {
  await open(page);
  const strip = page.locator(".rstrip");
  if (!isDesk(page)) { await expect(strip).toHaveCount(0); return; }   // phone: a list under the row
  await expect(strip.first()).toBeVisible();
  await strip.first().getByRole("radio", { name: /^720p/ }).click();
  await expect.poll(() => posts(sim, "/api/settings")).toEqual(["output.preset=3"]);
});

test("picker writes the source: a card on PC, the row's list on the phone", async ({ page, sim }) => {
  await open(page);
  const r = row(page, "menu:input/source");
  if (isDesk(page)) await r.getByRole("radio", { name: "Component", exact: true }).click();
  else {
    await r.getByRole("button", { name: /^Source:/ }).click();
    await page.getByRole("listbox", { name: "Source" }).getByRole("option", { name: /^Component/ }).click();
  }
  await expect.poll(() => posts(sim, "/api/settings")).toEqual(["input.source=4"]);
  await expect(page.locator(".plist")).toHaveCount(0);   // the phone's list closes on a choice
});

test("pad runs the move actions", async ({ page, sim }) => {
  await open(page);
  const r = row(page, "menu:screen/position");   // the arrows sit in the row, the readout between them
  await r.getByRole("button", { name: "Position up" }).click();
  await r.getByRole("button", { name: "Position left" }).click();
  await expect.poll(async () => [...await posts(sim, "/api/actions/screen.move_up"), ...await posts(sim, "/api/actions/screen.move_left")]).toHaveLength(2);
  await expect(r.locator(".pad2 b")).toHaveText(["140", "44"]);
});

test("action with confirm asks first", async ({ page, sim }) => {
  await open(page);
  const btn = row(page, "menu:picture/reset-picture").getByRole("button", { name: "Reset" });
  await btn.click();
  const dialog = page.getByRole("dialog", { name: /Reset picture/ });
  await expect(dialog).toBeVisible();
  await dialog.getByRole("button", { name: "Cancel" }).click();
  await expect(dialog).toHaveCount(0);
  expect(await posts(sim, "/api/actions/picture.reset")).toHaveLength(0);
  await btn.click();
  await page.getByRole("dialog").getByRole("button", { name: "Reset" }).click();
  await expect.poll(() => posts(sim, "/api/actions/picture.reset")).toHaveLength(1);
});

test("help sheet opens and closes with Escape", async ({ page }) => {
  await open(page);
  await row(page, "menu:picture/filter").getByRole("button", { name: "Help: Filter" }).click();
  const dialog = page.getByRole("dialog", { name: "Filter" });
  await expect(dialog).toContainText("How the edges of pixels are drawn");
  await expect(dialog.locator(".prev")).toBeVisible();   // the Filter help has its pictures
  await page.keyboard.press("Escape");
  await expect(page.getByRole("dialog")).toHaveCount(0);
});

test("greyed row shows the reason", async ({ page, sim }) => {
  await sim.post("/__sim/scenario", { name: "pro-svideo-ntsc" });
  await sim.post("/__sim/tv", { key: "adv.i2p", value: "0" });
  await open(page, "?mode=advanced");
  const r = row(page, "menu:input/2x-smooth");
  await expect(r).toHaveClass(/dis/);
  await expect(r.locator(".why")).toHaveText("Turn 2X on first");
  await expect(r.getByRole("switch")).toBeDisabled();
});

test("rows hidden by cable", async ({ page }) => {
  await open(page, "?mode=advanced");                       // RGB SCART: 2X smooth needs S-Video or Composite
  await expect(row(page, "menu:input/2x-smooth")).toHaveCount(0);
  await expect(row(page, "menu:picture/brightness")).toBeVisible();
});

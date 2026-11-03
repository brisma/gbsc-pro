// Settings page: Input, Output, Picture, Screen
import { test, expect, expectConsoleError } from "./fixtures.ts";
import type { Page } from "@playwright/test";
import type { SimHandle } from "./fixtures.ts";

const isDesk = (page: Page) => (page.viewportSize()?.width ?? 0) >= 960;
const posts = async (sim: SimHandle, path: string) =>
  (await sim.requests()).requests.filter((r) => r.method === "POST" && r.path === path).map((r) => r.body);
async function open(page: Page, hash: string, mode: "easy" | "advanced" = "easy"): Promise<void> {
  await page.addInitScript((m) => localStorage.setItem("gbsc.mode", JSON.stringify(m)), mode);
  await page.goto("/" + hash);
  await expect(page.locator(".ui[data-ready]")).toBeVisible();
}
const row = (page: Page, ref: string) => page.locator(`[data-row$=":${ref}"]`).first();

test("settings opens on Input", async ({ page }) => {
  await open(page, "#/settings");
  await expect(page).toHaveURL(/#\/settings\/input$/);
  await expect(page.getByRole("heading", { name: "Input", exact: true })).toBeVisible();
});

test("PC cable strip, phone Source list", async ({ page, sim }) => {
  await open(page, "#/settings/input");
  if (isDesk(page)) {
    const strip = page.getByRole("radiogroup", { name: "Source" });
    await expect(strip.getByRole("radio")).toHaveCount(7);
    await expect(strip.getByRole("radio", { name: "RGB SCART" })).toHaveAccessibleDescription(/60 Hz/);   // the signal line
    await strip.getByRole("radio", { name: "VGA" }).click();
    await expect.poll(() => posts(sim, "/api/settings")).toEqual(["input.source=3"]);
  } else {
    await row(page, "menu:input/source").getByRole("button", { name: /^Source:/ }).click();
    await page.getByRole("listbox", { name: "Source" }).getByRole("option", { name: /^Component/ }).click();
    await expect.poll(() => posts(sim, "/api/settings")).toEqual(["input.source=4"]);
  }
});

test("In Auto switches in Easy too: on the cable cards (PC), under Auto search (phone)", async ({ page, sim }) => {
  await open(page, "#/settings/input");
  const scanSwitch = page.getByRole("switch", { name: "VGA in Auto" });
  if (isDesk(page)) {
    await scanSwitch.click();
    await expect.poll(() => posts(sim, "/api/settings")).toEqual(["input.scan_vga=0"]);
  } else {
    await row(page, "menu:input/auto-search").getByRole("button", { name: /^Auto search:/ }).click();
    await page.locator(".inl").getByRole("switch", { name: "VGA" }).click();
    await expect.poll(() => posts(sim, "/api/settings")).toEqual(["input.scan_vga=0"]);
  }
});

test("Wait before search only in Advanced, 2 s by default", async ({ page, sim }) => {
  await open(page, "#/settings/input");
  await expect(row(page, "menu:input/wait-before-search")).toHaveCount(0);
  await page.getByRole("radiogroup", { name: "Mode" }).getByRole("radio", { name: /^Adv/ }).click();
  const r = row(page, "menu:input/wait-before-search");
  await expect(r.getByRole("radio", { name: "2s" })).toHaveAttribute("aria-checked", "true");
  await r.getByRole("radio", { name: "5s" }).click();
  await expect.poll(() => posts(sim, "/api/settings")).toEqual(["input.lost_wait=5"]);
});

test("75 ohm sync greyed outside RGB SCART and refused text on 409", async ({ page, sim }) => {
  await sim.post("/__sim/scenario", { name: "pro-vga" });
  await open(page, "#/settings/input", "advanced");
  const r = row(page, "menu:input/75-ohm-sync");
  await expect(r).toHaveClass(/dis/);
  await expect(r.locator(".why")).toHaveText("RGB SCART input only");
  await sim.post("/__sim/scenario", { name: "pro-rgbs" });
  await expect(r).not.toHaveClass(/dis/, { timeout: 5000 });
  // the input changed under the page before its next poll: the device refuses
  expectConsoleError(page, /409/);
  await page.route("**/api/actions/input.termination", (rt) => rt.fulfill({ status: 409, contentType: "application/json", body: '{"error":"not_rgb_scart"}' }));
  await r.getByRole("switch").click();
  await expect(page.locator(".toast")).toHaveText(/RGB SCART input only/);
});

test("75 ohm sync turns on with its readback", async ({ page, sim }) => {
  await open(page, "#/settings/input", "advanced");
  const sw = row(page, "menu:input/75-ohm-sync").getByRole("switch");
  await expect(sw).toHaveAttribute("aria-checked", "false");
  await sw.click();
  await expect.poll(() => posts(sim, "/api/actions/input.termination")).toEqual(["data=1"]);
  await expect(sw).toHaveAttribute("aria-checked", "true");
});

test("decoder group only on S-Video/Composite with the ADV Controller", async ({ page, sim }) => {
  await open(page, "#/settings/input", "advanced");
  await expect(page.getByText("Decoder", { exact: true })).toHaveCount(0);
  await expect(page.getByText(/Video standard, 2X and Decoder appear/)).toBeVisible();
  await sim.post("/__sim/scenario", { name: "pro-svideo-ntsc" });
  const band = page.getByRole("heading", { name: "Decoder", exact: true });   // the band that opens the group
  await expect(band).toBeVisible({ timeout: 5000 });
  await expect(row(page, "menu:input/decoder/brightness")).toBeVisible();
  await expect(page.getByText(/Video standard, 2X and Decoder appear/)).toHaveCount(0);
  await page.getByRole("radiogroup", { name: "Mode" }).getByRole("radio", { name: "Easy" }).click();
  await expect(band).toHaveCount(0);
});

test("scanlines cards: Easy four, Advanced seven", async ({ page }) => {
  await open(page, "#/settings/picture");
  await expect(page.getByRole("radiogroup", { name: "Scanlines" }).getByRole("radio")).toHaveCount(4);
  await page.getByRole("radiogroup", { name: "Mode" }).getByRole("radio", { name: /^Adv/ }).click();
  await expect(page.getByRole("radiogroup", { name: "Scanlines" }).getByRole("radio")).toHaveCount(7);
});

test("gain stepper shows the live value", async ({ page, sim }) => {
  await open(page, "#/settings/input", "advanced");
  const g = row(page, "menu:input/sampling/gain");
  await expect(g.locator(".stp b")).toHaveText("132");
  await sim.post("/__sim/live", { name: "adc_gain", value: 140 });
  await expect(g.locator(".stp b")).toHaveText("140", { timeout: 4000 });
  await g.getByRole("button", { name: "Increase Gain" }).click();
  await expect.poll(() => posts(sim, "/api/actions/input.gain_up")).toHaveLength(1);
});

test("screen pad holds to repeat screen.move_left", async ({ page, sim }) => {
  await open(page, "#/settings/scaler", "advanced");
  const left = row(page, "menu:screen/position").getByRole("button", { name: "Position left" });
  await left.scrollIntoViewIfNeeded();
  await left.hover();
  await page.mouse.down();
  await page.waitForTimeout(1100);
  await page.mouse.up();
  await expect.poll(async () => (await posts(sim, "/api/actions/screen.move_left")).length, { timeout: 5000 }).toBeGreaterThanOrEqual(3);
});

test("reset picture asks then posts picture.reset", async ({ page, sim }) => {
  await open(page, "#/settings/picture");
  await page.locator("[data-section=picture] .chead").getByRole("button", { name: /^Reset picture/ }).click();   // a button of the card
  await page.getByRole("dialog").getByRole("button", { name: "Reset" }).click();
  await expect.poll(() => posts(sim, "/api/actions/picture.reset")).toHaveLength(1);
});

test("tv change updates the row in place", async ({ page, sim }) => {
  await open(page, "#/settings/picture");
  const section = page.locator("[data-section=picture]");
  const id = await section.getAttribute("data-render-id");
  const b = row(page, "menu:picture/brightness");
  await sim.post("/__sim/tv", { key: "picture.brightness", value: "77" });
  await expect(b.getByRole("button", { name: /^Brightness 77/ })).toBeVisible();
  await sim.post("/__sim/tv", { key: "picture.scanlines", value: "1" });   // bridged: no echo, rev reload
  await expect(page.getByRole("radiogroup", { name: "Scanlines" }).getByRole("radio", { name: "Off" })).toHaveAttribute("aria-checked", "false");
  expect(await section.getAttribute("data-render-id")).toBe(id);
});

test("an open accordion survives a re-render of its group", async ({ page, sim }) => {
  await sim.post("/__sim/scenario", { name: "pro-composite-ntsc" });
  await open(page, "#/settings/input", "advanced");
  const ace = row(page, "menu:input/decoder/adaptive-contrast").getByRole("button", { name: /^Adaptive contrast:/ });
  await ace.click();
  await expect(ace).toHaveAttribute("aria-expanded", "true");
  const mode = page.getByRole("radiogroup", { name: "Mode" });
  await mode.getByRole("radio", { name: "Easy" }).click();     // the Decoder group goes away...
  await expect(ace).toHaveCount(0);
  await mode.getByRole("radio", { name: /^Adv/ }).click();     // ...and comes back as a new element
  await expect(ace).toHaveAttribute("aria-expanded", "true");
  await page.goto("/#/settings/picture");                       // another section closes it
  await expect(page).toHaveURL(/picture$/);
  await page.goto("/#/settings/input");
  await expect(row(page, "menu:input/decoder/adaptive-contrast").getByRole("button", { name: /^Adaptive contrast:/ })).toHaveAttribute("aria-expanded", "false");
});

test("adaptive contrast rows write their settings and Reset adaptive", async ({ page, sim }) => {
  await sim.post("/__sim/scenario", { name: "pro-composite-ntsc" });
  await open(page, "#/settings/input", "advanced");
  await row(page, "menu:input/decoder/adaptive-contrast").getByRole("button", { name: /^Adaptive contrast:/ }).click();
  const inl = page.locator(".inl");
  await inl.getByRole("switch", { name: "Enabled" }).click();
  await expect.poll(() => posts(sim, "/api/settings")).toEqual(["adv.ace=1"]);
  await inl.getByRole("button", { name: "Increase Contrast boost" }).click();
  await expect.poll(async () => (await posts(sim, "/api/settings")).length).toBe(2);
  expect((await posts(sim, "/api/settings"))[1]).toMatch(/^adv\.ace_luma_gain=\d+$/);
  await inl.locator('[data-row$="reset-adaptive"]').getByRole("button").last().click();
  const dialog = page.getByRole("dialog");
  if (await dialog.count()) await dialog.locator(".btn.pri").click();
  await expect.poll(() => posts(sim, "/api/actions/adv.ace_defaults")).toEqual([""]);
});

test("analog out picks RGB or YPbPr", async ({ page, sim }) => {
  await open(page, "#/settings/output", "advanced");
  const group = row(page, "menu:output/analog-out").getByRole("radiogroup", { name: "Analog out" });
  await expect(group.getByRole("radio", { name: "RGB", exact: true })).toHaveAttribute("aria-checked", "true");
  await group.getByRole("radio", { name: "YPbPr", exact: true }).click();
  await expect.poll(() => posts(sim, "/api/settings")).toEqual(["output.component=1"]);
});

test("red, green and blue rows wear their own colour", async ({ page }) => {
  await open(page, "#/settings/picture", "advanced");
  const colour = (ref: string) => row(page, ref).locator(".ic .ms").evaluate((e) => getComputedStyle(e).color);
  const [r, g, b] = [await colour("menu:picture/red"), await colour("menu:picture/green"), await colour("menu:picture/blue")];
  const rgb = (s: string) => s.match(/\d+/g)!.slice(0, 3).map(Number);
  expect(rgb(r)[0]).toBeGreaterThan(rgb(r)[1] + 80);
  expect(rgb(g)[1]).toBeGreaterThan(rgb(g)[0] + 60);
  expect(rgb(b)[2]).toBeGreaterThan(rgb(b)[0] + 80);
});

test("auto search says how many inputs it looks at", async ({ page, sim }) => {
  test.skip(isDesk(page), "the PC shows the inputs strip instead");
  await open(page, "#/settings/input", "advanced");
  const val = row(page, "menu:input/auto-search").getByRole("button", { name: /^Auto search:/ });
  await expect(val).toHaveAccessibleName("Auto search: 6 inputs");
  await sim.post("/__sim/tv", { key: "input.scan_av", value: "0" });
  await expect(val).toHaveAccessibleName("Auto search: 5 of 6");
});

test("phone: the sections are tabs, the open one is a card that says what it is for", async ({ page }) => {
  test.skip(isDesk(page), "the PC lists the sections in the sidebar");
  await open(page, "#/settings/input");
  const tabs = page.getByRole("navigation", { name: "Settings sections" });
  await expect(tabs.getByRole("link")).toHaveText(["Input", "Scaler", "Picture", "Output"]);
  await expect(tabs.getByRole("link", { name: "Input" })).toHaveAttribute("aria-current", "location");
  await expect(page.locator("[data-section=input] .chead p")).toHaveText("The cable and its signal");
  await tabs.getByRole("link", { name: "Output" }).click();
  await expect(page).toHaveURL(/#\/settings\/output$/);
  await expect(page.locator("[data-section=output] .chead p")).toHaveText("What goes to the TV");
  await expect(page.locator("[data-section=input]")).toHaveCount(0);
});

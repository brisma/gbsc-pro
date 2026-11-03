// Status page, three cards on one page: Console (category filter with the debug lines, pause, copy, download, clear;
// it follows the newest line until the reader scrolls up), Signal and history, Diagnostics with Copy for an issue
import { test, expect, allowNavigationAborts } from "./fixtures.ts";
import type { Page } from "@playwright/test";
import { readFileSync } from "node:fs";

async function open(page: Page): Promise<void> {
  await page.goto("/#/status");
  await expect(page.locator(".ui[data-ready]")).toBeVisible();
}
const card = (page: Page, id: "console" | "signal" | "diagnostics") => page.locator(`[data-section="${id}"]`);
const log = (page: Page) => card(page, "console").locator(".log");

test("status shows the console first, then signal and diagnostics, on phone and PC", async ({ page }) => {
  await open(page);
  await expect(page).toHaveURL(/#\/status$/);
  await expect(page.locator("[data-section] > .chead h2")).toHaveText(["Console", "Signal and history", "Diagnostics"]);
  await expect(log(page)).toBeVisible();
  await expect(card(page, "diagnostics").getByRole("button", { name: "Copy for an issue" })).toBeVisible();
});

test("console shows log since start and live lines", async ({ page, sim }) => {
  await open(page);
  await expect(log(page)).toContainText("(simulator)");
  await sim.post("/__sim/log", { cat: "input", msg: "live line from the device" });
  await expect(log(page)).toContainText("live line from the device");
  await expect(log(page).locator("div", { hasText: "live line from the device" })).toHaveCount(1);
});

test("filter hides other categories", async ({ page, sim }) => {
  await open(page);
  await sim.post("/__sim/log", { cat: "input", msg: "an input line" });
  await sim.post("/__sim/log", { cat: "adv", msg: "an adv line" });
  await expect(log(page)).toContainText("an adv line");
  await page.getByRole("button", { name: /^Category/ }).click();
  await page.getByRole("option", { name: "input" }).click();
  await expect(log(page)).toContainText("an input line");
  await expect(log(page)).not.toContainText("an adv line");
});

// The console keeps the newest line in view until the reader scrolls up, and again once they are back at the bottom
test("console follows the newest line unless the reader scrolled up", async ({ page, sim }) => {
  await open(page);
  for (let i = 0; i < 60; i++) await sim.post("/__sim/log", { cat: "sys", msg: `filler line ${i}` });
  await expect(log(page)).toContainText("filler line 59");
  const atBottom = () => log(page).evaluate((b) => b.scrollHeight - b.scrollTop - b.clientHeight < 4);
  await expect.poll(atBottom).toBe(true);
  await sim.post("/__sim/log", { cat: "sys", msg: "newest while following" });
  await expect(log(page)).toContainText("newest while following");
  await expect.poll(atBottom).toBe(true);
  await log(page).evaluate((b) => { b.scrollTop = 0; b.dispatchEvent(new Event("scroll")); });   // the reader scrolls up
  await sim.post("/__sim/log", { cat: "sys", msg: "newest not following" });
  await expect(log(page)).toContainText("newest not following");
  expect(await log(page).evaluate((b) => b.scrollTop)).toBe(0);
  await log(page).evaluate((b) => { b.scrollTop = b.scrollHeight; b.dispatchEvent(new Event("scroll")); });   // back at the bottom
  await sim.post("/__sim/log", { cat: "sys", msg: "following again" });
  await expect(log(page)).toContainText("following again");
  await expect.poll(atBottom).toBe(true);
});

test("pause holds new lines and resume shows them", async ({ page, sim }) => {
  await open(page);
  await page.getByRole("button", { name: "Pause" }).click();
  await expect(page.getByRole("button", { name: "Resume" })).toHaveAttribute("aria-pressed", "true");
  await sim.post("/__sim/log", { cat: "sys", msg: "while paused" });
  await page.waitForTimeout(400);
  await expect(log(page)).not.toContainText("while paused");
  await page.getByRole("button", { name: "Resume" }).click();
  await expect(log(page)).toContainText("while paused");
});

test("debug lines can be hidden from the category menu", async ({ page, sim }) => {
  allowNavigationAborts(page);
  await open(page);
  await sim.post("/__sim/log", { cat: "api", level: "debug", msg: "a debug detail" });
  await page.reload();   // debug records come with their level from /api/log
  await expect(page.locator(".ui[data-ready]")).toBeVisible();
  await expect(log(page)).toContainText("a debug detail");
  await page.getByRole("button", { name: /^Category/ }).click();
  const opt = page.getByRole("option", { name: "Debug lines" });
  await expect(opt).toHaveAttribute("aria-selected", "true");
  await expect(opt).toContainText("Dev mode");
  await opt.click();
  await expect(log(page)).not.toContainText("a debug detail");
});

test("download gives the visible lines", async ({ page, sim }) => {
  await open(page);
  await sim.post("/__sim/log", { cat: "input", msg: "kept" });
  await sim.post("/__sim/log", { cat: "adv", msg: "filtered out" });
  await expect(log(page)).toContainText("filtered out");
  await page.getByRole("button", { name: /^Category/ }).click();
  await page.getByRole("option", { name: "input" }).click();
  const dl = page.waitForEvent("download");
  await page.getByRole("button", { name: "Download" }).click();
  const d = await dl;
  expect(d.suggestedFilename()).toMatch(/^gbsc-log.*\.txt$/);
  const text = readFileSync((await d.path())!, "utf8");
  expect(text).toMatch(/\[input\]\s+kept/);
  expect(text).not.toContain("filtered out");
});

test("clear empties the view only", async ({ page, sim }) => {
  await open(page);
  await expect(log(page)).toContainText("(simulator)");
  await page.getByRole("button", { name: "Clear" }).click();
  await expect(log(page)).not.toContainText("(simulator)");
  await sim.post("/__sim/log", { cat: "sys", msg: "after clear" });
  await expect(log(page)).toContainText("after clear");
});

test("history updates on e:", async ({ page, sim }) => {
  await open(page);
  const events = card(page, "signal").locator(".rw.ev");
  await sim.post("/__sim/event", { type: "input_lost", a: 6, b: 0 });
  await expect(events.first()).toContainText("Signal lost on Composite");
  await sim.post("/__sim/event", { type: "resolution", a: 0x15, b: 1 });
  await expect(events.first()).toContainText("Resolution: 1080p");   // newest first
});

test("copy for an issue has versions and crash", async ({ page }) => {
  await open(page);
  await card(page, "diagnostics").getByRole("button", { name: /Copy for an issue/ }).click();
  const box = page.getByRole("dialog", { name: "Copy for an issue" }).getByRole("textbox");
  await expect(box).toHaveValue(/GBS-Control 3\.0\.0-dev0/);
  await expect(box).toHaveValue(/Board: pro/);
  await expect(box).toHaveValue(/### Last crash/);
  await expect(box).toHaveValue(/### Console/);
});

test("signal shows input, output and the ADV Controller", async ({ page }) => {
  await open(page);
  const kv = card(page, "signal").locator(".facts");
  await expect(kv).toContainText("RGB SCART");
  await expect(kv).toContainText("240p");
  await expect(kv).toContainText("ADV Controller");
});

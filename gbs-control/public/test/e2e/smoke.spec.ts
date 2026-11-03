// Smoke on every project: the app loads, connects, stays accessible and fast
import { test, expect, expectConsoleError } from "./fixtures.ts";
import { expectA11y, measureFirstUsable } from "./checks.ts";

test("app loads and connects @device", async ({ page }) => {
  await page.goto("/");
  await expect(page.locator(".ui[data-ready]")).toBeVisible();
  await expect(page.locator(".sta")).toHaveAttribute("aria-label", /Stable/);
  await expect(page.getByRole("navigation", { name: "Pages" })).toBeVisible();
});

test("connection loss and recovery", async ({ page, sim }) => {
  expectConsoleError(page, /WebSocket/);
  await page.goto("/");
  await expect(page.locator(".sta")).toHaveAttribute("aria-label", /Stable/);
  await sim.post("/__sim/fault", { name: "wsDrop", on: true });
  await expect(page.locator(".sta")).toHaveAttribute("aria-label", /Reconnecting/);
  await expect(page.locator(".sta")).toHaveAttribute("aria-label", /Stable/, { timeout: 5000 });
});

test("the busy page on a route retries until the device answers", async ({ page, sim }) => {
  // a reload right after a restart, with little memory: the page must not stay on the busy text
  expectConsoleError(page, /503/);
  await page.goto("/#/status");
  await expect(page.locator(".sta")).toHaveAttribute("aria-label", /Stable/);
  await sim.post("/__sim/fault", { name: "busy503All", on: true });
  await page.reload();
  await expect(page.locator("body")).toContainText("busy, retrying");
  await sim.post("/__sim/fault", { name: "busy503All", on: false });
  await expect(page.locator(".sta")).toHaveAttribute("aria-label", /Stable/, { timeout: 10000 });
  expect(page.url()).toContain("#/status");
});

test("no a11y violations", async ({ page }) => {
  await page.goto("/");
  await expect(page.locator(".ui[data-ready]")).toBeVisible();
  await expectA11y(page);
});

test("first usable under 2 s", async ({ page, sim, browserName }) => {
  test.skip(browserName !== "chromium", "CPU throttling needs Chromium (CDP)");
  const t = await measureFirstUsable(page, sim, ".ui[data-ready]");
  console.log(`first usable: ${t.afterPage} ms after the page, ${t.total} ms in total (device network profile, CPU x4)`);
  expect(t.afterPage).toBeLessThan(2000);
});

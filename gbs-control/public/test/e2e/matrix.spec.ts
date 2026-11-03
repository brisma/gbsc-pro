// Cross-page gates: every page and section × Easy/Advanced/Dev × Pro/Standard renders without console errors (all 8
// projects for each page's default section, the phone projects and desktop-chromium-1280 for the rest); on phones
// nothing overflows, nothing is covered and tap targets are 44 px; axe on one engine per form factor; once more with
// the long-texts scenario; the first usable Quick screen.
import { test, expect } from "./fixtures.ts";
import type { Page, TestInfo } from "@playwright/test";
import type { SimHandle } from "./fixtures.ts";
import { PAGES } from "../../app/layout/pages.ts";
import { expectNoOverflow, expectNotCovered, expectTapTargets, expectA11y, measureFirstUsable } from "./checks.ts";

type Mode = "easy" | "advanced" | "dev";
const TRIMMED = ["phone-iphone15", "phone-pixel8", "phone-small", "desktop-chromium-1280"];
const A11Y = ["phone-pixel8", "desktop-chromium-1280"];
const phone = (info: TestInfo) => !!(info.project.metadata as { phone?: boolean } | undefined)?.phone;

// Routes: each page, and each section of the pages that have them (the phone-only link card has no route)
const ROUTES: { page: string; section: string | null; first: boolean }[] = [];
for (const p of PAGES) {
  const secs = p.sections.filter((s) => !s.to);
  if (!secs.length) ROUTES.push({ page: p.id, section: null, first: true });
  secs.forEach((s, i) => ROUTES.push({ page: p.id, section: s.id, first: i === 0 }));
}

async function setUp(page: Page, sim: SimHandle, scenario: string, mode: Mode): Promise<void> {
  await sim.post("/__sim/scenario", { name: scenario });
  if (mode === "dev") {   // applied in the device's loop: load the page with Dev already on
    await sim.post("/__sim/tv", { key: "system.devmode", value: "1" });
    await expect.poll(async () => (await (await page.request.get("/api/settings")).json())["system.devmode"]).toBe(1);
  }
  await page.addInitScript((m) => localStorage.setItem("gbsc.mode", JSON.stringify(m)), mode === "easy" ? "easy" : "advanced");
}

async function checks(page: Page, info: TestInfo): Promise<void> {
  await page.waitForTimeout(150);   // sections settle (accordions, live values)
  if (phone(info)) {
    await expectNoOverflow(page);
    await expectNotCovered(page);
    await expectTapTargets(page);
  }
  if (A11Y.includes(info.project.name)) await expectA11y(page);
}

for (const r of ROUTES)
  for (const board of ["pro", "standard"] as const)
    for (const mode of (r.page === "dev" ? ["dev"] : ["easy", "advanced", "dev"]) as Mode[])
      test(`matrix ${r.page}${r.section ? "/" + r.section : ""} ${mode} ${board}`, async ({ page, sim }, info) => {
        test.skip(!r.first && !TRIMMED.includes(info.project.name), "sections after the first: phone projects and desktop-chromium-1280");
        await setUp(page, sim, board === "standard" ? "standard-rgbs" : "pro-rgbs", mode);
        await page.goto(`/#/${r.page}${r.section ? "/" + r.section : ""}`);
        await expect(page.locator(".ui[data-ready]")).toBeVisible();
        await checks(page, info);
      });

for (const r of ROUTES)
  test(`matrix long texts ${r.page}${r.section ? "/" + r.section : ""}`, async ({ page, sim }, info) => {
    test.skip(!TRIMMED.includes(info.project.name), "long texts: phone projects and desktop-chromium-1280");
    await setUp(page, sim, "long-texts", r.page === "dev" ? "dev" : "advanced");
    await page.goto(`/#/${r.page}${r.section ? "/" + r.section : ""}`);
    await expect(page.locator(".ui[data-ready]")).toBeVisible();
    await checks(page, info);
  });

test("first usable Quick under 2 s", async ({ page, sim, browserName }, info) => {
  test.skip(browserName !== "chromium" || !phone(info), "Chromium phone projects (CDP CPU throttling)");
  const t = await measureFirstUsable(page, sim, '[data-section="quick"]');
  console.log(`first usable Quick: ${t.afterPage} ms after the page, ${t.total} ms in total (device network profile, CPU x4)`);
  expect(t.afterPage).toBeLessThan(2000);
});

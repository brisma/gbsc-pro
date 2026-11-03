// Quick page: the Shortcuts card (the menu's Quick items, Edit) and the card that loads a profile, under the chain
import { test, expect } from "./fixtures.ts";
import type { Page } from "@playwright/test";
import type { SimHandle } from "./fixtures.ts";
import menuJson from "../../gen/menu.json" with { type: "json" };

const isDesk = (page: Page) => (page.viewportSize()?.width ?? 0) >= 960;
const posts = async (sim: SimHandle, path: string) =>
  (await sim.requests()).requests.filter((r) => r.method === "POST" && r.path === path).map((r) => r.body);
const quick = (page: Page) => page.locator('[data-section="quick"]');
async function ready(page: Page, hash = ""): Promise<void> {
  await page.goto("/" + hash);
  await expect(page.locator(".ui[data-ready]")).toBeVisible();
  await expect(quick(page).locator(".rw").first()).toBeVisible();
}
const quickLabels = (page: Page) => quick(page).locator(".rw .lb > span:first-child").allInnerTexts();
// A choice of Source or Resolution: PC = a card of the strip, phone = the row opens its list
async function pick(page: Page, row: string, option: RegExp): Promise<void> {
  if (isDesk(page)) { await quick(page).getByRole("radiogroup", { name: row }).getByRole("radio", { name: option }).click(); return; }
  await quick(page).getByRole("button", { name: new RegExp(`^${row}:`) }).click();
  await page.getByRole("listbox", { name: row }).getByRole("option", { name: option }).click();
}

test("quick list follows menu.json quick in Easy and Advanced", async ({ page }) => {
  await ready(page);
  const menuQuick = (menuJson as any).quick.children.map((n: any) => n.label) as string[];
  // RGB SCART on a Pro board: 2X needs S-Video/Composite, the TV OSD row is standard-board only and excluded
  // ... and the Profile row is the card under the shortcuts
  const want = menuQuick.filter((l) => l !== "2X" && l !== "TV OSD" && l !== "Profile");
  want.push("All settings");
  expect((await quickLabels(page)).map((t) => t.split("\n")[0])).toEqual(want);
  await page.getByRole("radiogroup", { name: "Mode" }).getByRole("radio", { name: /^Adv/ }).click();
  expect((await quickLabels(page)).map((t) => t.split("\n")[0])).toEqual(want);
});

test("2X joins the list on S-Video", async ({ page, sim }) => {
  await sim.post("/__sim/scenario", { name: "pro-svideo-ntsc" });
  await ready(page);
  expect((await quickLabels(page)).map((t) => t.split("\n")[0])).toContain("2X");
});

test("source change writes input.source", async ({ page, sim }) => {
  await ready(page);
  await pick(page, "Source", /^VGA/);
  await expect.poll(() => posts(sim, "/api/settings")).toEqual(["input.source=3"]);
});

test("resolution card writes the choice in one request and confirms after the rev reload", async ({ page, sim }) => {
  await ready(page);
  const before = (await sim.requests()).requests.length;
  await pick(page, "Resolution", /^720p/);
  await expect.poll(() => posts(sim, "/api/settings")).toEqual(["output.preset=3"]);
  if (isDesk(page)) await expect(quick(page).getByRole("radio", { name: /^720p/ })).toHaveAttribute("aria-checked", "true");
  else await expect(quick(page).getByRole("button", { name: /^Resolution:/ })).toHaveAccessibleName("Resolution: 720p");
  // bridged output.preset: no WebSocket echo; the page reloads the settings after status.rev
  await expect.poll(async () => {
    const after = (await sim.requests()).requests.slice(before);
    const p = after.findIndex((r) => r.method === "POST");
    return p >= 0 && after.slice(p + 1).some((r) => r.method === "GET" && r.path === "/api/settings");
  }).toBe(true);
});

test("the state chip follows the search and opens Status", async ({ page, sim }) => {
  await ready(page);
  const chip = page.locator(".sta");
  await expect(chip).toHaveAttribute("aria-label", "Signal: Stable");
  await expect(page.locator(".chain .st").first()).toContainText("RGB SCART");
  await sim.post("/__sim/scenario", { name: "pro-auto-search" });
  await expect(chip).toHaveAttribute("aria-label", "Signal: Searching inputs", { timeout: 5000 });
  await sim.post("/__sim/scenario", { name: "pro-no-signal" });
  await expect(chip).toHaveAttribute("aria-label", "Signal: No signal", { timeout: 5000 });
  await chip.click();
  await expect(page).toHaveURL(/#\/status/);
});

test("chain stages open their Settings section", async ({ page }) => {
  await ready(page);
  for (const [i, id] of [[0, "input"], [1, "scaler"], [2, "picture"], [3, "output"]] as const) {
    await page.goto("/#/quick");
    await page.locator(".chain .st").nth(i).click();
    await expect(page).toHaveURL(new RegExp(`#/settings/${id}$`));
  }
});

test("the chain names the profile, the unsaved notice takes its place", async ({ page, sim }) => {
  await ready(page);
  await expect(page.locator(".chainblk .pf")).toHaveAccessibleName(/^Profile: /);
  await sim.post("/__sim/scenario", { name: "unsaved-profile" });
  await page.reload();
  await expect(page.locator(".notice")).toContainText("unsaved changes");
  await expect(page.locator(".chainblk .pf")).toHaveCount(0);
});

test("Load a profile: the active one tagged and without Load, All profiles", async ({ page, sim }) => {
  await sim.post("/__sim/scenario", { name: "unsaved-profile" });
  await ready(page);
  await expect(quick(page).getByText("Profile", { exact: true })).toHaveCount(0);
  const card = page.locator('[data-section="quick-profiles"]');
  await expect(card.getByRole("heading", { name: "Load a profile" })).toBeVisible();
  await expect(card.locator(".rw").filter({ hasText: "Active" })).toHaveCount(1);
  await expect(card.locator(".rw").filter({ hasText: "Active" }).getByRole("button")).toHaveCount(0);
  await card.getByRole("link", { name: /All profiles/ }).click();
  await expect(page).toHaveURL(/#\/profiles/);
});

test("edit adds a row from all settings and removes one", async ({ page }) => {
  await ready(page);
  await page.getByRole("button", { name: "Edit" }).click();
  await quick(page).getByRole("button", { name: "Pin Brightness" }).click();
  await quick(page).getByRole("button", { name: "Unpin Filter" }).click();
  await page.getByRole("button", { name: "Done" }).click();
  const labels = (await quickLabels(page)).map((t) => t.split("\n")[0]);
  expect(labels).toContain("Brightness");
  expect(labels).not.toContain("Filter");
  const pins = JSON.parse((await page.evaluate(() => localStorage.getItem("gbsc.quickPins"))) ?? "[]");
  expect(pins).toContain("picture/brightness");
  expect(pins).not.toContain("quick/filter");
});

test("edit reorders with up and down", async ({ page }) => {
  await ready(page);
  await page.getByRole("button", { name: "Edit" }).click();
  await quick(page).getByRole("button", { name: "Move Resolution up" }).click();
  await page.getByRole("button", { name: "Done" }).click();
  const labels = (await quickLabels(page)).map((t) => t.split("\n")[0]);
  expect(labels.indexOf("Resolution")).toBeLessThan(labels.indexOf("Source"));
});

test("Shortcuts shows the Filter and Scanlines choices open on PC and phone", async ({ page }) => {
  await ready(page);
  const list = quick(page);
  await expect(list.getByRole("radiogroup", { name: "Filter" })).toBeVisible();
  await expect(list.getByRole("radiogroup", { name: "Scanlines" })).toBeVisible();
});

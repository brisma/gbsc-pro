// App shell: navigation, mode switch, header strip, remote, banners, sheets, start-up under load
import { test, expect, expectConsoleError, allowNavigationAborts } from "./fixtures.ts";
import type { Page } from "@playwright/test";
import type { SimHandle } from "./fixtures.ts";

const isDesk = (page: Page) => (page.viewportSize()?.width ?? 0) >= 960;
const posts = async (sim: SimHandle, path: string) =>
  (await sim.requests()).requests.filter((r) => r.method === "POST" && r.path === path).map((r) => r.body);
const ready = async (page: Page, hash = "") => {
  await page.goto("/" + hash);
  await expect(page.locator(".ui[data-ready]")).toBeVisible();
};
const pages = (page: Page) => page.getByRole("navigation", { name: "Pages" });

test("tabs and sidebar navigate to every page", async ({ page }) => {
  await ready(page);
  for (const [name, id] of [["Settings", "settings"], ["Profiles", "profiles"], ["Status", "status"], ["System", "system"], ["Quick", "quick"]]) {
    await pages(page).getByRole("link", { name, exact: true }).click();
    await expect(page).toHaveURL(new RegExp(`#/${id}`));
    await expect(pages(page).getByRole("link", { name, exact: true })).toHaveAttribute("aria-current", "page");
  }
});

test("page route opens the first section", async ({ page }) => {
  allowNavigationAborts(page);
  await ready(page, "#/settings");
  await expect(page).toHaveURL(/#\/settings\/input$/);
  await page.goto("/#/system");
  await expect(page).toHaveURL(/#\/system\/wifi$/);
  if (isDesk(page)) await expect(pages(page).getByRole("link", { name: "WiFi", exact: true })).toHaveAttribute("aria-current", "location");
  await page.goto("/#/nowhere");
  await expect(page).toHaveURL(/#\/quick$/);
});

test("mode switch persists per browser", async ({ page }) => {
  allowNavigationAborts(page);
  await ready(page);
  const mode = page.getByRole("radiogroup", { name: "Mode" });
  await expect(mode.getByRole("radio", { name: "Easy" })).toHaveAttribute("aria-checked", "true");
  await mode.getByRole("radio", { name: /^Adv/ }).click();
  await expect(mode.getByRole("radio", { name: /^Adv/ })).toHaveAttribute("aria-checked", "true");
  await page.reload();
  await expect(page.getByRole("radiogroup", { name: "Mode" }).getByRole("radio", { name: /^Adv/ })).toHaveAttribute("aria-checked", "true");
  expect(await page.evaluate(() => localStorage.getItem("gbsc.mode"))).toBe('"advanced"');
});

test("Dev writes system.devmode and not menu_mode", async ({ page, sim }) => {
  await ready(page);
  await page.getByRole("radiogroup", { name: "Mode" }).getByRole("radio", { name: "Dev" }).click();
  await expect(page.getByRole("radiogroup", { name: "Mode" }).getByRole("radio", { name: "Dev" })).toHaveAttribute("aria-checked", "true");
  await expect.poll(() => posts(sim, "/api/settings")).toEqual(["system.devmode=1"]);
  if (isDesk(page)) await expect(pages(page).getByRole("link", { name: "Developer", exact: true })).toBeVisible();
  expect((await sim.requests()).requests.some((r) => r.body.includes("menu_mode"))).toBe(false);
});

test("leaving Dev on Developer goes to System", async ({ page, sim }) => {
  await ready(page);
  allowNavigationAborts(page);
  await page.getByRole("radiogroup", { name: "Mode" }).getByRole("radio", { name: "Dev" }).click();
  await page.goto("/#/dev");
  await expect(page).toHaveURL(/#\/dev\/sync$/);
  await page.getByRole("radiogroup", { name: "Mode" }).getByRole("radio", { name: "Easy" }).click();
  await expect(page).toHaveURL(/#\/system\//);
  await expect.poll(() => posts(sim, "/api/settings")).toEqual(["system.devmode=1", "system.devmode=0"]);
});

// The open section: its card's heading (PC and phone)
const openSection = (page: Page, name: string) => page.getByRole("heading", { name, exact: true });

test("mode switch keeps the open section", async ({ page }) => {
  await ready(page, "#/settings/picture");
  await expect(openSection(page, "Picture")).toBeVisible();
  await page.getByRole("radiogroup", { name: "Mode" }).getByRole("radio", { name: /^Adv/ }).click();
  await expect(page).toHaveURL(/#\/settings\/picture$/);
  await expect(openSection(page, "Picture")).toBeVisible();
});

test("Save profile only with unsaved changes", async ({ page }) => {
  await ready(page);
  await expect(page.locator(".sta")).toHaveAttribute("aria-label", /Stable/);
  await expect(page.getByRole("button", { name: "Save profile" })).toHaveCount(0);
});

test("Save profile and Discard with unsaved changes, on Profiles too", async ({ page, sim }) => {
  await sim.post("/__sim/scenario", { name: "unsaved-profile" });
  await ready(page);
  const save = page.getByRole("button", { name: "Save profile" });
  await expect(save).toBeVisible();
  await expect(page.locator(".notice")).toContainText("unsaved");
  await pages(page).getByRole("link", { name: "Profiles", exact: true }).click();
  await expect(save).toBeVisible();
  await pages(page).getByRole("link", { name: "Quick", exact: true }).click();
  await save.click();
  await expect.poll(async () => (await posts(sim, "/api/profiles/save")).length).toBe(1);
  expect((await posts(sim, "/api/profiles/save"))[0]).toMatch(/^id=p\d\d$/);
});

test("Discard changes asks first, then loads the active profile again", async ({ page, sim }) => {
  await sim.post("/__sim/scenario", { name: "unsaved-profile" });
  await ready(page);
  await page.getByRole("button", { name: "Discard changes", exact: true }).click();
  await page.getByRole("button", { name: "Cancel" }).click();
  expect((await posts(sim, "/api/profiles/load")).length).toBe(0);
  await page.getByRole("button", { name: "Discard changes", exact: true }).click();
  await page.getByRole("dialog").getByRole("button", { name: "Discard changes" }).click();
  await expect.poll(async () => (await posts(sim, "/api/profiles/load")).length).toBe(1);
  expect((await posts(sim, "/api/profiles/load"))[0]).toMatch(/^id=p\d\d$/);
});

test("Update available with an offer (PC header), hidden on System Update, badge on System", async ({ page, sim }) => {
  await sim.post("/__sim/scenario", { name: "update-offer" });
  await ready(page);
  await expect(pages(page).getByRole("link", { name: /^System/ })).toHaveAccessibleName(/update available/);
  const upd = page.locator("a.upd");   // the header's own link (the System entry also says "update available")
  if (!isDesk(page)) { await expect(upd).toHaveCount(0); return; }   // phone and tablet have the badge on their System tab only
  await expect(upd).toBeVisible();
  await upd.click();
  await expect(page).toHaveURL(/#\/system\/update$/);
  await expect(upd).toHaveCount(0);
});

test("no update offer: no button, no badge", async ({ page }) => {
  await ready(page);
  await expect(page.locator(".sta")).toHaveAttribute("aria-label", /Stable/);
  await expect(page.locator("a.upd")).toHaveCount(0);
  await expect(pages(page).getByRole("link", { name: "System", exact: true })).toBeVisible();
});

test("remote sends menu.key codes", async ({ page, sim }) => {
  await ready(page);
  await page.getByRole("button", { name: "Remote", exact: true }).click();
  const remote = page.getByRole("dialog", { name: "Remote" });
  for (const [key, code] of [["Exit", "05"], ["OK", "04"], ["Save", "08"], ["Vol +", "0A"], ["Up", "00"], ["Menu", "06"]]) {
    await remote.getByRole("button", { name: key, exact: true }).click();
    await expect.poll(async () => (await posts(sim, "/api/actions/menu.key")).at(-1)).toBe(`data=${code}`);
  }
  await expect(remote.getByRole("button", { name: "Quick" })).toHaveCount(0);   // the IR remote has no Quick key
});

test("remote on the standard board has no volume keys", async ({ page, sim }) => {
  await sim.post("/__sim/scenario", { name: "standard-rgbs" });
  await ready(page);
  await page.getByRole("button", { name: "Remote", exact: true }).click();
  await expect(page.getByRole("dialog", { name: "Remote" }).getByRole("button", { name: "OK", exact: true })).toBeVisible();
  await expect(page.getByRole("dialog", { name: "Remote" }).getByRole("button", { name: "Vol +" })).toHaveCount(0);
});

test("remote arrow holds to repeat", async ({ page, sim }) => {
  await ready(page);
  await page.getByRole("button", { name: "Remote", exact: true }).click();
  const dialog = page.getByRole("dialog", { name: "Remote" });
  await dialog.evaluate((el) => Promise.all(el.getAnimations().map((a) => a.finished)));   // the sheet is in place
  const up = dialog.getByRole("button", { name: "Up", exact: true });
  await up.hover();
  await page.mouse.down();
  await page.waitForTimeout(1100);   // first at once, again after 380 ms, then every 300 ms
  await page.mouse.up();
  // the keys queue behind the start-up requests (2 in flight): wait for them to arrive
  await expect.poll(async () => (await posts(sim, "/api/actions/menu.key")).length, { timeout: 5000 }).toBeGreaterThanOrEqual(3);
  await page.waitForTimeout(500);
  expect((await posts(sim, "/api/actions/menu.key")).length).toBeLessThanOrEqual(5);
  await expect(dialog).toBeVisible();   // the press landed on the key, not on the shade
});

test("start-up survives busy503 for 3 s", async ({ page, sim }) => {
  expectConsoleError(page, /503/);
  await sim.post("/__sim/fault", { name: "busy503", on: true });
  setTimeout(() => void sim.post("/__sim/fault", { name: "busy503", on: false }), 3000);
  await page.goto("/");
  await expect(page.locator(".ui[data-ready]")).toBeVisible({ timeout: 15000 });
  await expect(page.locator(".sta")).toHaveAttribute("aria-label", /Stable/);
});

test("reconnect catches up", async ({ page, sim }) => {
  expectConsoleError(page, /WebSocket/);
  await ready(page);
  await expect(page.locator(".sta")).toHaveAttribute("aria-label", /Stable/);
  const before = (await sim.requests()).requests.length;
  await sim.post("/__sim/fault", { name: "wsDrop", on: true });
  await expect(page.getByText("Connection lost")).toBeVisible();
  await expect(page.locator(".sta")).toHaveAttribute("aria-label", /Reconnecting/);
  await expect(page.locator(".sta")).toHaveAttribute("aria-label", /Stable/, { timeout: 8000 });
  await expect(page.getByText("Connection lost")).toHaveCount(0);
  // the catch-up requests leave two at a time (the page's in-flight limit): wait for all of them, a slow engine is late
  const after = async () => (await sim.requests()).requests.slice(before).map((r) => r.path.split("?")[0]);
  await expect.poll(after).toEqual(expect.arrayContaining(["/api/settings", "/api/log", "/api/events", "/api/profiles"]));
});

test("Escape closes sheets and dialogs", async ({ page }) => {
  await ready(page);
  await page.getByRole("button", { name: "Remote", exact: true }).click();
  await expect(page.getByRole("dialog", { name: "Remote" })).toBeVisible();
  await page.keyboard.press("Escape");
  await expect(page.getByRole("dialog")).toHaveCount(0);
  await expect(page.getByRole("button", { name: "Remote", exact: true })).toBeFocused();   // focus goes back
});

test("focus order follows the visual order in the header and tabs", async ({ page, browserName }) => {
  test.skip(browserName === "webkit", "WebKit tabs only to form fields by default (the Safari setting)");
  await ready(page);
  const names: string[] = [];
  for (let i = 0; i < 60; i++) {
    await page.keyboard.press("Tab");
    names.push(await page.evaluate(() => {
      const el = document.activeElement as HTMLElement | null;
      return (el?.getAttribute("aria-label") || el?.textContent || "").replace(/[\uE000-\uF8FF]/g, "").trim();   // icon glyphs
    }));
  }
  const want = isDesk(page)
    ? ["Quick", "Settings", "Profiles", "Status", "System", "Remote", "Easy"]
    : ["Easy", "Quick", "Settings", "Profiles", "Status", "System", "Remote"];
  const at = want.map((w) => names.findIndex((n) => n === w || n.startsWith(w + ",")));
  expect(at.every((x) => x >= 0), names.join(" | ")).toBe(true);
  expect([...at].sort((a, b) => a - b), names.join(" | ")).toEqual(at);
});

// Add to Home Screen (PWA): the old page carried a manifest for Add to Home Screen; no service worker on a plain-http LAN
test("add to home screen: the page carries a manifest", async ({ page }) => {
  await ready(page);
  const href = await page.locator('link[rel="manifest"]').getAttribute("href");
  expect(href).toMatch(/^data:application\/manifest\+json/);
  const m = JSON.parse(decodeURIComponent(href!.slice(href!.indexOf(",") + 1)));
  expect(m.name).toBe("GBSC PRO");
  expect(m.display).toBe("standalone");
  expect(m.icons[0].src).toMatch(/^data:image\/png;base64,/);   // the 16-colour PNG: the loader stays in the first 8 KB
  await expect(page.locator('meta[name="apple-mobile-web-app-capable"]')).toHaveAttribute("content", "yes");
  await expect(page.locator('link[rel="apple-touch-icon"]')).toHaveCount(1);
});

// A page of a few seconds over the device's WiFi is fine, but it must show it is loading: the loader
// comes in the first bytes of the HTML (before the big style and script) and goes when the app is ready
test("a loader shows while the page arrives and goes when the app is ready", async ({ page, sim, browserName }) => {
  test.skip(browserName !== "chromium", "one engine is enough for the order of the HTML");
  const html = await (await page.request.get("/")).text();
  const boot = html.indexOf('id="boot"');
  expect(boot).toBeGreaterThan(0);
  expect(boot).toBeLessThan(8192);   // after the head (manifest icons ~5 KB, under 3 KB on the wire: ~0.1 s at 25 KB/s)
  expect(boot).toBeLessThan(html.indexOf('<div id="app">'));
  await sim.post("/__sim/net", { profile: "device" });                  // ~25 KB/s as measured on the unit
  await page.goto("/", { waitUntil: "commit" });
  await expect(page.locator("#boot")).toBeVisible({ timeout: 2000 });
  await expect(page.getByRole("progressbar", { name: "Loading GBSC PRO" })).toBeVisible();   // not a second "status" next to the header's
  await expect(page.locator(".ui[data-ready]")).toBeVisible({ timeout: 20000 });
  await expect(page.locator("#boot")).toHaveCount(0);
});

// The device stops sending (WiFi blip) while the socket stays open
test("a silent link shows Reconnecting within 3 s and catches up when messages return", async ({ page, sim }) => {
  expectConsoleError(page, /WebSocket/);
  await ready(page);
  await expect(page.locator(".sta")).toHaveAttribute("aria-label", /Stable/);
  await sim.post("/__sim/fault", { name: "wsSilent", on: true });
  await expect(page.locator(".sta")).toHaveAttribute("aria-label", /Reconnecting/, { timeout: 3500 });
  await sim.post("/__sim/fault", { name: "wsSilent", on: false });
  await expect(page.locator(".sta")).toHaveAttribute("aria-label", /Stable/, { timeout: 12000 });
});

// Busy for longer than the client's own retries; the page still ends up complete
test("start-up survives busy503 for 9 s", async ({ page, sim }) => {
  expectConsoleError(page, /503/);
  await sim.post("/__sim/fault", { name: "busy503", on: true });
  setTimeout(() => void sim.post("/__sim/fault", { name: "busy503", on: false }), 9000);
  await page.goto("/");
  await expect(page.locator(".ui[data-ready]")).toBeVisible({ timeout: 30000 });
  await expect(page.locator("#boot")).toHaveCount(0);
  await expect(page.locator(".sta")).toHaveAttribute("aria-label", /Stable/);
});

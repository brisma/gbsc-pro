// Profiles page: the list (Load on each row, the row's actions in a menu on PC and a sheet on the phone), new profile
// with console and region, import/export, what the device does when a signal arrives
import { test, expect, allowNavigationAborts } from "./fixtures.ts";
import type { Page } from "@playwright/test";
import type { SimHandle } from "./fixtures.ts";
import { readFileSync } from "node:fs";
import { zipRead, zipWrite } from "../../app/model/zip.ts";

const isDesk = (page: Page) => (page.viewportSize()?.width ?? 0) >= 960;
const api = (sim: SimHandle, method: string, path: string, form?: Record<string, string>) =>
  fetch(sim.url + path, { method, headers: { "X-GBSC": "1", Host: "gbscontrol.local" }, body: form ? new URLSearchParams(form) : undefined });
async function mk(sim: SimHandle, name: string, region = "PAL"): Promise<void> {
  await api(sim, "POST", "/api/profiles/saveas", { name, region });
  await new Promise((r) => setTimeout(r, 120));   // the op runs after opDelayMs
}
const list = async (sim: SimHandle) => (await api(sim, "GET", "/api/profiles")).json();
const postsSince = async (sim: SimHandle, n: number, path: string) =>
  (await sim.requests()).requests.slice(n).filter((r) => r.method === "POST" && r.path === path).map((r) => r.body);
const mark = async (sim: SimHandle) => (await sim.requests()).requests.length;
async function open(page: Page): Promise<void> {
  await page.goto("/#/profiles");
  await expect(page.locator(".ui[data-ready]")).toBeVisible();
}
const esc = (t: string) => t.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
const exact = (t: string) => new RegExp(`^${esc(t)}( · [A-Za-z0-9-]+)?$`);   // the name, with its region if any
const base = (t: string) => t.split(" · ")[0];   // a row prints the name and, beside it, the region
const prow = (page: Page, name: string) => page.locator(".prow").filter({ has: page.locator(".lb > span:first-child", { hasText: new RegExp(`^${esc(base(name))}$`) }) }).first();
// The actions of a profile: the row's menu (PC) or the sheet that comes up (phone), both a list of menu items
async function actions(page: Page, name: string) {
  await page.getByRole("button", { name: new RegExp(`^Actions for ${esc(base(name))}( · [A-Za-z0-9-]+)?$`) }).click();
  return page.getByRole("menu");
}
const loadFromList = (page: Page, name: string) => prow(page, name).getByRole("button", { name: /^Load / }).click();
const newProfile = (page: Page) => page.getByRole("button", { name: isDesk(page) ? "New profile" : "New", exact: true }).click();

test("Load on a profile's row loads it", async ({ page, sim }) => {
  await mk(sim, "One"); await mk(sim, "Two");   // Two is active
  await open(page);
  const n = await mark(sim);
  await expect(prow(page, "Two").getByRole("button", { name: /^Load / })).toHaveCount(0);   // the active one has no Load
  await loadFromList(page, "One");
  await expect.poll(() => postsSince(sim, n, "/api/profiles/load")).toHaveLength(1);
  await expect(prow(page, "One")).toContainText("Active");
});

test("new profile with a console chip and region goes active", async ({ page, sim }) => {
  await open(page);
  const n = await mark(sim);
  await newProfile(page);
  const d = page.getByRole("dialog", { name: "New profile" });
  await d.getByRole("button", { name: "Saturn", exact: true }).click();
  await expect(d.getByRole("textbox", { name: "Name" })).toHaveValue("Saturn");
  await d.getByRole("radio", { name: "NTSC-J" }).click();
  await d.getByRole("button", { name: "Save" }).click();
  await expect.poll(() => postsSince(sim, n, "/api/profiles/saveas")).toEqual(["name=Saturn&region=NTSC-J"]);
  await expect(prow(page, "Saturn · NTSC-J")).toContainText("Active");
});

test("same signal prompt replaces or keeps both", async ({ page, sim }) => {
  await mk(sim, "Mega Drive");
  await open(page);
  let n = await mark(sim);
  await newProfile(page);
  let d = page.getByRole("dialog", { name: "New profile" });
  await d.getByRole("textbox", { name: "Name" }).fill("Genesis");
  await expect(d.getByText(/Same signal as Mega Drive/)).toBeVisible();
  await d.getByRole("button", { name: "Keep both" }).click();
  await expect.poll(() => postsSince(sim, n, "/api/profiles/saveas")).toEqual([expect.stringMatching(/^name=Genesis&region=/)]);
  await expect.poll(async () => (await list(sim)).profiles.length).toBe(2);
  n = await mark(sim);
  await newProfile(page);
  d = page.getByRole("dialog", { name: "New profile" });
  await d.getByRole("textbox", { name: "Name" }).fill("Mega Drive 2");
  await d.getByRole("button", { name: "Replace it" }).click();
  await expect.poll(() => postsSince(sim, n, "/api/profiles/replace")).toEqual([expect.stringMatching(/^id=p\d\d&name=Mega\+Drive\+2&region=/)]);
  expect((await list(sim)).profiles.length).toBe(2);
});

test("load with unsaved changes asks Save or Discard", async ({ page, sim }) => {
  await mk(sim, "One"); await mk(sim, "Two");
  await api(sim, "POST", "/api/settings", { "picture.brightness": "60" });   // Two is active and now modified
  await open(page);
  const notice = page.locator(".notice");
  await expect(notice).toContainText("unsaved");
  let n = await mark(sim);
  await loadFromList(page, "One");
  let d = page.getByRole("dialog", { name: "Unsaved changes" });
  await d.getByRole("button", { name: "Discard" }).click();
  await expect.poll(() => postsSince(sim, n, "/api/profiles/load")).toHaveLength(1);
  expect(await postsSince(sim, n, "/api/profiles/save")).toHaveLength(0);
  await expect(prow(page, "One · PAL")).toContainText("Active");            // the load finished
  await expect(notice).toHaveCount(0);
  await api(sim, "POST", "/api/settings", { "picture.brightness": "61" });   // One modified
  await expect(notice).toContainText("unsaved");
  n = await mark(sim);
  await loadFromList(page, "Two");
  d = page.getByRole("dialog", { name: "Unsaved changes" });
  await d.getByRole("button", { name: "Save" }).click();
  await expect.poll(() => postsSince(sim, n, "/api/profiles/save")).toHaveLength(1);
  await expect.poll(() => postsSince(sim, n, "/api/profiles/load")).toHaveLength(1);
});

test("save current settings into another profile", async ({ page, sim }) => {
  await mk(sim, "One"); await mk(sim, "Two");   // Two active
  await open(page);
  const n = await mark(sim);
  const a = await actions(page, "One");
  await a.getByRole("menuitem", { name: /Save current settings here/ }).click();
  await page.getByRole("dialog", { name: /Save into One/ }).getByRole("button", { name: "Save" }).click();
  await expect.poll(() => postsSince(sim, n, "/api/profiles/save")).toEqual([expect.stringMatching(/^id=p\d\d$/)]);
});

test("replace with current", async ({ page, sim }) => {
  await mk(sim, "One");
  await open(page);
  const n = await mark(sim);
  const a = await actions(page, "One");
  await a.getByRole("menuitem", { name: /Replace with current/ }).click();
  await page.getByRole("dialog", { name: /Replace One/ }).getByRole("button", { name: "Replace" }).click();
  await expect.poll(() => postsSince(sim, n, "/api/profiles/replace")).toEqual([expect.stringMatching(/^id=p\d\d$/)]);
});

test("rename and duplicate", async ({ page, sim }) => {
  await mk(sim, "One");
  await open(page);
  let n = await mark(sim);
  let a = await actions(page, "One");
  await a.getByRole("menuitem", { name: "Rename" }).click();
  const d = page.getByRole("dialog", { name: "Rename" });
  await d.getByRole("textbox", { name: "Name" }).fill("Uno");
  await d.getByRole("button", { name: "Rename" }).click();
  await expect.poll(() => postsSince(sim, n, "/api/profiles/rename")).toEqual([expect.stringMatching(/^id=p\d\d&name=Uno$/)]);
  await expect(prow(page, "Uno")).toBeVisible();
  n = await mark(sim);
  a = await actions(page, "Uno");
  await a.getByRole("menuitem", { name: "Duplicate" }).click();
  await expect.poll(() => postsSince(sim, n, "/api/profiles/duplicate")).toHaveLength(1);
  await expect(prow(page, "Uno 2")).toBeVisible();
});

test("forget signal only with a fingerprint", async ({ page, sim }) => {
  await mk(sim, "One");
  await open(page);
  let a = await actions(page, "One");
  await a.getByRole("menuitem", { name: "Forget signal" }).click();
  await expect.poll(async () => (await list(sim)).profiles[0].fp).toBe("");
  a = await actions(page, "One");
  await expect(a.getByRole("menuitem", { name: "Forget signal" })).toHaveCount(0);
});

test("delete asks first", async ({ page, sim }) => {
  await mk(sim, "One");
  await open(page);
  const n = await mark(sim);
  const a = await actions(page, "One");
  await a.getByRole("menuitem", { name: "Delete" }).click();
  const d = page.getByRole("dialog", { name: /Delete One/ });
  await d.getByRole("button", { name: "Cancel" }).click();
  expect(await postsSince(sim, n, "/api/profiles/delete")).toHaveLength(0);
  const a2 = await actions(page, "One");
  await a2.getByRole("menuitem", { name: "Delete" }).click();
  await page.getByRole("dialog", { name: /Delete One/ }).getByRole("button", { name: "Delete" }).click();
  await expect.poll(async () => (await list(sim)).profiles.length).toBe(0);
});

test("import then export round trip", async ({ page, sim }) => {
  await mk(sim, "SNES", "NTSC-J");
  await open(page);
  const a = await actions(page, "SNES");
  const dl = page.waitForEvent("download");
  await a.getByRole("menuitem", { name: "Export" }).click();
  const file = await (await dl).path();
  const text = readFileSync(file!, "utf8");
  expect(text).toMatch(/^schema=1\nprofile\.name=SNES\n/);
  await api(sim, "POST", "/api/profiles/delete", { id: (await list(sim)).profiles[0].id });
  await new Promise((r) => setTimeout(r, 120));
  await page.reload();
  allowNavigationAborts(page);
  await expect(page.locator(".ui[data-ready]")).toBeVisible();
  await page.locator("input[type=file][data-import]").setInputFiles({ name: "SNES.txt", mimeType: "text/plain", buffer: Buffer.from(text) });
  const s = page.getByRole("dialog", { name: "Import" });
  await expect(s).toContainText("1 imported");
  await expect.poll(async () => (await list(sim)).profiles.map((p: any) => p.name)).toEqual(["SNES"]);
});

test("export all gives a zip with every profile", async ({ page, sim }) => {
  await mk(sim, "One"); await mk(sim, "Two"); await mk(sim, "Three");
  await open(page);
  const dl = page.waitForEvent("download");
  await page.getByRole("button", { name: /Export all/ }).click();   // PC: a button of the card; phone: under the list
  const d = await dl;
  expect(d.suggestedFilename()).toMatch(/\.zip$/);
  const entries = await zipRead(new Uint8Array(readFileSync((await d.path())!)));
  expect(entries.map((e) => e.name).sort()).toEqual(["One.gbscprofile.txt", "Three.gbscprofile.txt", "Two.gbscprofile.txt"]);
  expect(new TextDecoder().decode(entries.find((e) => e.name === "Two.gbscprofile.txt")!.data)).toMatch(/profile\.name=Two/);
});

const prof = (name: string) => new TextEncoder().encode(`schema=1\nprofile.name=${name}\nprofile.region=PAL\nprofile.modes=pal\n`);

test("import a zip of three profiles reports each", async ({ page, sim }) => {
  await open(page);
  const z = zipWrite([{ name: "a.txt", data: prof("Alpha") }, { name: "b.txt", data: prof("Beta") }, { name: "c.txt", data: new TextEncoder().encode("junk") }]);
  await page.locator("input[type=file][data-import]").setInputFiles({ name: "three.zip", mimeType: "application/zip", buffer: Buffer.from(z) });
  const s = page.getByRole("dialog", { name: "Import" });
  await expect(s).toContainText("2 imported");
  await expect(s).toContainText("c.txt");             // the skipped one, with its reason
  await expect(s).toContainText(/not a GBSC PRO profile/i);
  expect((await list(sim)).profiles.map((p: any) => p.name).sort()).toEqual(["Alpha", "Beta"]);
});

test("import several txt files", async ({ page, sim }) => {
  await open(page);
  await page.locator("input[type=file][data-import]").setInputFiles([
    { name: "x.txt", mimeType: "text/plain", buffer: Buffer.from(prof("Xeno")) },
    { name: "y.txt", mimeType: "text/plain", buffer: Buffer.from(prof("Yoshi")) },
  ]);
  await expect(page.getByRole("dialog", { name: "Import" })).toContainText("2 imported");
  expect((await list(sim)).profiles.map((p: any) => p.name).sort()).toEqual(["Xeno", "Yoshi"]);
});

test("36 profiles show the limit warning", async ({ page, sim }) => {
  await sim.post("/__sim/scenario", { name: "profiles-full" });
  await open(page);
  await expect(page.getByText("36 saved of 36")).toBeVisible();
  const a = await actions(page, "Game 1");
  await a.getByRole("menuitem", { name: "Duplicate" }).click();
  await expect(page.locator(".perr")).toContainText("Profiles full: 36 of 36");
});

test("the active profile's row carries Save and Discard, like the notice", async ({ page, sim }) => {
  await sim.post("/__sim/scenario", { name: "unsaved-profile" });
  await open(page);
  await expect(page.locator(".notice").getByRole("button", { name: "Save profile" })).toBeVisible();
  const active = page.locator(".prow").filter({ hasText: "Active" });
  if (isDesk(page)) await expect(active.getByRole("button", { name: /^Discard the changes to / })).toBeVisible();
  else {   // phone: Discard is in the row's sheet
    await active.getByRole("button", { name: /^Actions for / }).click();
    await expect(page.getByRole("menuitem", { name: "Discard changes" })).toBeVisible();
    await page.keyboard.press("Escape");
  }
  const n = await mark(sim);
  await active.getByRole("button", { name: /^Save/ }).click();
  await expect.poll(() => postsSince(sim, n, "/api/profiles/save")).toHaveLength(1);
});

test("with no profile the card explains how profiles work", async ({ page, sim }) => {
  await open(page);
  await expect(page.getByRole("heading", { name: "How profiles work" })).toBeVisible();
  await mk(sim, "One");
  await page.reload();
  await expect(page.locator(".ui[data-ready]")).toBeVisible();
  await expect(prow(page, "One")).toBeVisible();
  await expect(page.getByRole("heading", { name: "How profiles work" })).toHaveCount(0);
});

test("when a signal arrives writes auto-load, load sets input and start-up", async ({ page, sim }) => {
  await page.addInitScript(() => localStorage.setItem("gbsc.mode", '"advanced"'));   // the last two are Advanced
  await open(page);
  const n = await mark(sim);
  await page.locator('[data-row$=":menu:profiles/auto-load"]').getByRole("switch").click();
  await page.locator('[data-row$=":menu:profiles/load-sets-input"]').getByRole("switch").click();
  await page.locator('[data-row$=":menu:profiles/startup"]').getByRole("radio", { name: "Defaults" }).click();
  await expect.poll(() => postsSince(sim, n, "/api/settings")).toEqual(["profiles.autoload=0", "profiles.switch_input=0", "profiles.startup=1"]);   // both switches start on
});

test("Defaults loads the factory picture settings", async ({ page, sim }) => {
  await mk(sim, "Mega Drive");
  await open(page);
  const n = await mark(sim);
  await page.getByRole("button", { name: "Load Defaults" }).click();
  const dialog = page.getByRole("dialog");
  if (await dialog.count()) await dialog.locator(".btn.pri").click();
  await expect.poll(async () => (await postsSince(sim, n, "/api/profiles/defaults")).length).toBe(1);
});

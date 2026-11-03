// Developer › Scaler · TV5725 › Registers: the 677 fields of tv5725.h by name, read by segment (dev.tv5725_read +
// /api/dev/regs), edited as toggles, steppers or hex, written as planned triplets (dev.tv5725_reg)
import { test, expect } from "./fixtures.ts";
import type { Page } from "@playwright/test";
import type { SimHandle } from "./fixtures.ts";
import { tvReg } from "../../sim/routes-dev.ts";

async function open(page: Page, sim: SimHandle): Promise<void> {
  await sim.post("/__sim/tv", { key: "system.devmode", value: "1" });
  await expect.poll(async () => (await (await page.request.get("/api/settings")).json())["system.devmode"]).toBe(1);
  await page.addInitScript(() => localStorage.setItem("gbsc.mode", '"advanced"'));
  await page.goto("/#/dev/registers");
  await expect(page.locator(".ui[data-ready]")).toBeVisible();
  await expect(regs(page).getByRole("searchbox", { name: "Register name" })).toBeVisible();
}
const regs = (page: Page) => page.locator('[data-section="registers"]');
const posts = async (sim: SimHandle, path: string) =>
  (await sim.requests()).requests.filter((r) => r.method === "POST" && r.path === path).map((r) => decodeURIComponent(r.body.replace(/\+/g, " ")));
const field = (page: Page, name: string) => regs(page).locator(`[data-field="${name}"]`);
const hscale = () => { const b16 = tvReg(3, 0x16), b17 = tvReg(3, 0x17); return ((b17 << 8) | b16) & 0x3ff; };
async function segment(page: Page, n: number): Promise<void> {
  await regs(page).getByRole("radiogroup", { name: "Segment" }).getByRole("radio", { name: new RegExp(`^S${n}\\b`) }).click();
}

test("search finds a field by part of its name", async ({ page, sim }) => {
  await open(page, sim);
  await regs(page).getByRole("searchbox", { name: "Register name" }).fill("hscale");
  await expect(field(page, "VDS_HSCALE")).toBeVisible();
  await expect(field(page, "VDS_HSCALE")).toContainText("3_16");
  await expect(field(page, "VDS_VSCALE")).toHaveCount(0);
});

test("unread segment shows a dash and a read button", async ({ page, sim }) => {
  await open(page, sim);
  await regs(page).getByRole("searchbox", { name: "Register name" }).fill("VDS_HSCALE");
  await expect(field(page, "VDS_HSCALE")).toContainText("—");
  await regs(page).getByRole("button", { name: "Read segment 3" }).click();
  await expect.poll(() => posts(sim, "/api/actions/dev.tv5725_read")).toEqual(["data=03"]);
  await expect(field(page, "VDS_HSCALE")).toContainText(String(hscale()));
});

test("read shows values of the segment", async ({ page, sim }) => {
  await open(page, sim);
  await segment(page, 3);
  await regs(page).getByRole("button", { name: "Read segment 3" }).click();
  await expect(field(page, "VDS_HSCALE")).toContainText(String(hscale()));
});

test("one long list, segments are anchors, aliases hidden", async ({ page, sim }) => {
  await open(page, sim);
  for (let n = 0; n < 6; n++) await expect(regs(page).locator(".regsec", { hasText: `Segment ${n} ·` })).toHaveCount(1);
  await expect(field(page, "SFTRST_IF_RSTZ")).toHaveCount(1);         // every segment in the list, no "more"
  await expect(field(page, "VDS_HSCALE")).toHaveCount(1);
  await expect(field(page, "RESET_CONTROL_0x46")).toHaveCount(0);     // the whole-register alias is hidden
  await segment(page, 5);
  await expect(regs(page).locator(".regsec", { hasText: "Segment 5 ·" })).toBeInViewport();
  await regs(page).getByRole("searchbox", { name: "Register name" }).fill("RESET_CONTROL");
  await expect(field(page, "RESET_CONTROL_0x46")).toBeVisible();     // search still finds it
});

test("busy during an update is explained", async ({ page, sim }) => {
  await open(page, sim);
  await sim.post("/__sim/fault", { name: "regsBusy" });
  await segment(page, 3);
  await regs(page).getByRole("button", { name: "Read segment 3" }).click();
  await expect(regs(page)).toContainText("firmware update");
});

test("pending bar writes the planned triplets and re-reads", async ({ page, sim }) => {
  await open(page, sim);
  await segment(page, 3);
  await regs(page).getByRole("button", { name: "Read segment 3" }).click();
  await expect(field(page, "VDS_HSCALE")).toContainText(String(hscale()));
  await field(page, "VDS_HSCALE").getByRole("button", { name: "Increase VDS_HSCALE" }).click();
  const bar = regs(page).locator(".pbar");
  await expect(bar).toContainText("1 change");
  await bar.getByRole("button", { name: "Write" }).click();
  const v = hscale() + 1, b17 = (tvReg(3, 0x17) & ~0x03) | (v >> 8);
  const hx = (n: number) => n.toString(16).toUpperCase().padStart(2, "0");
  await expect.poll(() => posts(sim, "/api/actions/dev.tv5725_reg")).toEqual([`data=03,16,${hx(v & 0xff)},03,17,${hx(b17)}`]);
  await expect.poll(async () => (await posts(sim, "/api/actions/dev.tv5725_read")).length).toBe(3);   // read, again before the write, after it
  await expect(field(page, "VDS_HSCALE")).toContainText(String(v));
  await expect(bar).toHaveCount(0);
});

test("read-only fields cannot be edited", async ({ page, sim }) => {
  await open(page, sim);
  await regs(page).getByRole("searchbox", { name: "Register name" }).fill("STATUS_IF_HT_OK");
  await expect(field(page, "STATUS_IF_HT_OK")).toBeVisible();
  await expect(field(page, "STATUS_IF_HT_OK").getByRole("switch")).toBeDisabled();
});

test("soft reset asks first", async ({ page, sim }) => {
  await open(page, sim);
  await regs(page).getByRole("searchbox", { name: "Register name" }).fill("SFTRST_IF_RSTZ");
  await regs(page).getByRole("button", { name: "Read segment 0" }).click();
  await field(page, "SFTRST_IF_RSTZ").getByRole("switch").click();
  await regs(page).locator(".pbar").getByRole("button", { name: "Write" }).click();
  const box = page.getByRole("dialog", { name: /reset/i });
  await expect(box).toBeVisible();
  await box.getByRole("button", { name: "Cancel" }).click();
  expect(await posts(sim, "/api/actions/dev.tv5725_reg")).toEqual([]);
});

test("write reads the segment again first: bits the device changed since are kept", async ({ page, sim }) => {
  await open(page, sim);
  await segment(page, 3);
  await regs(page).getByRole("button", { name: "Read segment 3" }).click();
  await expect(field(page, "VDS_HSCALE")).toContainText(String(hscale()));
  await field(page, "VDS_HSCALE").getByRole("button", { name: "Increase VDS_HSCALE" }).click();
  await page.request.post("/api/actions/dev.tv5725_reg", { headers: { "X-GBSC": "1" }, form: { data: "03,17,FF" } });   // the firmware moves VDS_VSCALE meanwhile
  await regs(page).locator(".pbar").getByRole("button", { name: "Write" }).click();
  const v = hscale() + 1, hx = (n: number) => n.toString(16).toUpperCase().padStart(2, "0");
  await expect.poll(async () => (await posts(sim, "/api/actions/dev.tv5725_reg")).slice(-1))
    .toEqual([`data=03,16,${hx(v & 0xff)},03,17,${hx(0xfc | (v >> 8))}`]);
});

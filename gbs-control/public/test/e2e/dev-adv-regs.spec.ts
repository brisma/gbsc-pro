// Developer › ADV Controller › Decoder · ADV7280 and Encoder · ADV7391: the known options of the ADV Manager maps,
// values read on open (dev.adv_read, one at a time), changes applied together through the planner (dev.adv_i2c) and
// read again
import { test, expect } from "./fixtures.ts";
import type { Page } from "@playwright/test";
import type { SimHandle } from "./fixtures.ts";
import { advReg } from "../../sim/routes-dev.ts";

async function open(page: Page, sim: SimHandle, section = "decoder"): Promise<void> {
  await sim.post("/__sim/tv", { key: "system.devmode", value: "1" });
  await expect.poll(async () => (await (await page.request.get("/api/settings")).json())["system.devmode"]).toBe(1);
  await page.addInitScript(() => localStorage.setItem("gbsc.mode", '"advanced"'));
  await page.goto(`/#/dev/${section}`);
  await expect(page.locator(".ui[data-ready]")).toBeVisible();
}
const sec = (page: Page, id = "decoder") => page.locator(`[data-section="${id}"]`);
const posts = async (sim: SimHandle, path: string) =>
  (await sim.requests()).requests.filter((r) => r.method === "POST" && r.path === path).map((r) => decodeURIComponent(r.body.replace(/\+/g, " ")));
const reg = (page: Page, key: string, id = "decoder") => sec(page, id).locator(`[data-areg="${key}"]`);
async function find(page: Page, q: string, id = "decoder"): Promise<void> {
  await sec(page, id).getByRole("searchbox", { name: "Search options" }).fill(q);
}

test("values come from a read", async ({ page, sim }) => {
  await open(page, sim);
  await expect.poll(async () => (await posts(sim, "/api/actions/dev.adv_read")).length).toBeGreaterThan(0);
  expect((await posts(sim, "/api/actions/dev.adv_read"))[0]).toMatch(/^data=42,00,00,[0-9A-F]{2}$/);
  await find(page, "brightness");
  await expect(reg(page, "42:00:0a")).toContainText(String(advReg(0x42, 0, 0x0a)));
});

test("apply sends planned groups then re-reads", async ({ page, sim }) => {
  await open(page, sim);
  await find(page, "brightness");
  const v = advReg(0x42, 0, 0x0a);
  await expect(reg(page, "42:00:0a")).toContainText(String(v));
  const reads = (await posts(sim, "/api/actions/dev.adv_read")).length;
  await reg(page, "42:00:0a").getByRole("button", { name: /^Increase BRI/ }).click();
  const bar = sec(page).locator(".pbar");
  await expect(bar).toContainText("1 register");
  await bar.getByRole("button", { name: "Apply" }).click();
  const hx = (n: number) => n.toString(16).toUpperCase().padStart(2, "0");
  await expect.poll(() => posts(sim, "/api/actions/dev.adv_i2c")).toEqual([`data=42,0A,${hx(v + 1)}`]);
  await expect.poll(async () => (await posts(sim, "/api/actions/dev.adv_read")).length).toBeGreaterThan(reads);
  await expect(reg(page, "42:00:0a")).toContainText(String(v + 1));
  await expect(bar).toHaveCount(0);
});

test("apply without a reply shows unconfirmed", async ({ page, sim }) => {
  await open(page, sim);
  await find(page, "brightness");
  await expect(reg(page, "42:00:0a")).toContainText(String(advReg(0x42, 0, 0x0a)));
  await sim.post("/__sim/fault", { name: "advNoReply" });
  await reg(page, "42:00:0a").getByRole("button", { name: /^Increase BRI/ }).click();
  await sec(page).locator(".pbar").getByRole("button", { name: "Apply" }).click();
  await expect(sec(page)).toContainText("not confirmed");
  await expect(sec(page)).not.toContainText("Applied");
});

test("dangerous register write asks with the warning", async ({ page, sim }) => {
  await open(page, sim);
  await find(page, "PWRDWN");
  await expect(reg(page, "42:00:0f").getByRole("switch", { name: /PWRDWN/ })).toBeEnabled();
  await reg(page, "42:00:0f").getByRole("switch", { name: /PWRDWN/ }).click();
  await sec(page).locator(".pbar").getByRole("button", { name: "Apply" }).click();
  const box = page.getByRole("dialog");
  await expect(box).toContainText("the picture stays wrong until the input changes or the device restarts");
  await box.getByRole("button", { name: "Cancel" }).click();
  expect(await posts(sim, "/api/actions/dev.adv_i2c")).toEqual([]);
});

test("managed register is marked", async ({ page, sim }) => {
  await open(page, sim);
  await find(page, "brightness");
  await expect(reg(page, "42:00:0a")).toContainText("The device sets this");
});

test("apply disabled during an ADV Controller update", async ({ page, sim }) => {
  await open(page, sim);
  await find(page, "brightness");
  await expect(reg(page, "42:00:0a")).toContainText(String(advReg(0x42, 0, 0x0a)));
  await reg(page, "42:00:0a").getByRole("button", { name: /^Increase BRI/ }).click();
  await sim.post("/__sim/fault", { name: "advUpdateHold" });   // the image copy takes seconds on the device
  await page.request.post("/api/actions/system.update_adv_controller", { headers: { "X-GBSC": "1" } });
  await expect(sec(page).locator(".pbar").getByRole("button", { name: "Apply" })).toBeDisabled();
});

test("old image shows the update hint", async ({ page, sim }) => {
  await sim.post("/__sim/scenario", { name: "adv-old-image" });
  await open(page, sim);
  await expect(sec(page)).toContainText("Update the ADV Controller");
  await expect(sec(page).getByRole("searchbox", { name: "Search options" })).toHaveCount(0);
});

test("vpp error does not block the decoder map", async ({ page, sim }) => {
  await sim.post("/__sim/fault", { name: "vppNoReply" });
  await open(page, sim);
  // the maps are read one after the other (each read polled every 120 ms): under load the VPP one comes late
  await expect(sec(page).locator('[data-amap="84:00"]')).toContainText("No answer", { timeout: 15_000 });
  await find(page, "brightness");
  await expect(reg(page, "42:00:0a")).toContainText(String(advReg(0x42, 0, 0x0a)));
});

test("encoder section reads the ADV7391", async ({ page, sim }) => {
  await open(page, sim, "encoder");
  await expect.poll(async () => (await posts(sim, "/api/actions/dev.adv_read")).some((b) => b.startsWith("data=56,00,"))).toBe(true);
  await find(page, "RGB_YPBPR_SEL", "encoder");
  await expect(reg(page, "56:00:02", "encoder")).toBeVisible();
});

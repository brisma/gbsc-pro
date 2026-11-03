// Developer › ADV Controller › Manual and console: one register with its bits, and the console with the ADV Controller
// CLI syntax (writes through the link, reads and dumps through dev.adv_read)
import { test, expect } from "./fixtures.ts";
import type { Page } from "@playwright/test";
import type { SimHandle } from "./fixtures.ts";
import { advReg } from "../../sim/routes-dev.ts";

async function open(page: Page, sim: SimHandle): Promise<void> {
  await sim.post("/__sim/tv", { key: "system.devmode", value: "1" });
  await expect.poll(async () => (await (await page.request.get("/api/settings")).json())["system.devmode"]).toBe(1);
  await page.addInitScript(() => localStorage.setItem("gbsc.mode", '"advanced"'));
  await page.goto("/#/dev/manual");
  await expect(page.locator(".ui[data-ready]")).toBeVisible();
}
const sec = (page: Page) => page.locator('[data-section="manual"]');
const hx = (n: number) => n.toString(16).toUpperCase().padStart(2, "0");
const posts = async (sim: SimHandle, path: string) =>
  (await sim.requests()).requests.filter((r) => r.method === "POST" && r.path === path).map((r) => decodeURIComponent(r.body.replace(/\+/g, " ")));
async function send(page: Page, cmd: string): Promise<void> {
  const box = sec(page).getByRole("textbox", { name: "Command" });
  await box.fill(cmd);
  await box.press("Enter");
}
const log = (page: Page) => sec(page).getByRole("log", { name: "ADV console" });

test("console read prints the value", async ({ page, sim }) => {
  await open(page, sim);
  await send(page, "r 42 0A");
  await expect(log(page)).toContainText(`42 0A = 0x${hx(advReg(0x42, 0, 0x0a))}`);
});

test("console dump prints 16 per line", async ({ page, sim }) => {
  await open(page, sim);
  await send(page, "d 42 00 20");
  const lines = log(page).locator(".l.dump");
  await expect(lines).toHaveCount(2);
  await expect(lines.first()).toHaveText(`00: ${Array.from({ length: 16 }, (_, i) => hx(advReg(0x42, 0, i))).join(" ")}`);
});

test("write to a dangerous register asks first", async ({ page, sim }) => {
  await open(page, sim);
  await send(page, "w 56 17 02");
  const box = page.getByRole("dialog");
  await expect(box).toContainText("the picture stays wrong until the input changes or the device restarts");
  await box.getByRole("button", { name: "Cancel" }).click();
  await send(page, "w 42 0E 40");
  await expect(log(page)).toContainText("refused: map select");
  expect(await posts(sim, "/api/actions/dev.adv_i2c")).toEqual([]);
  await send(page, "w 42 0A 10");
  await expect.poll(() => posts(sim, "/api/actions/dev.adv_i2c")).toEqual(["data=42,0A,10"]);
});

test("bit toggles write the byte", async ({ page, sim }) => {
  await open(page, sim);
  const reg = sec(page).getByRole("button", { name: "Next register" });
  for (let i = 0; i < 10; i++) await reg.click();
  await expect(sec(page).locator(".mreg")).toContainText("0x0A");
  await sec(page).getByRole("button", { name: "Read", exact: true }).click();
  const v = advReg(0x42, 0, 0x0a);
  await expect(sec(page).locator(".mval")).toHaveText(`0x${hx(v)}`);
  await sec(page).getByRole("switch", { name: "Bit 7" }).click();
  await sec(page).getByRole("button", { name: "Write", exact: true }).click();
  await expect.poll(() => posts(sim, "/api/actions/dev.adv_i2c")).toEqual([`data=42,0A,${hx(v ^ 0x80)}`]);
});

test("the register's fields are decoded under the bits", async ({ page, sim }) => {
  await open(page, sim);
  await sec(page).getByRole("button", { name: "Read", exact: true }).click();   // 0x00 Input Control
  const v = advReg(0x42, 0, 0x00);
  const fields = sec(page).locator(".mfields");
  await expect(fields).toContainText("INSEL");
  await expect(fields).toContainText("bits 4–0");
  await expect(fields).toContainText(String(v & 0x1f));
  await sec(page).getByRole("switch", { name: "Bit 0" }).click();               // the decode follows the bits
  await expect(fields).toContainText(String((v ^ 1) & 0x1f));
});

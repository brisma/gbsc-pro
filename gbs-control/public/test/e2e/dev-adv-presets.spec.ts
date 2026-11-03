// Developer › ADV Controller › Presets and macros: stored on the device as ADV Manager .yaml, saved from the values
// applied now, applied through the planner, macros built from the known options
import { test, expect, expectConsoleError } from "./fixtures.ts";
import type { Page } from "@playwright/test";
import type { SimHandle } from "./fixtures.ts";
import { readFileSync } from "node:fs";
import { join } from "node:path";

const ADV = join(import.meta.dirname, "..", "..", "..", "..", "shared", "adv-regmap");
async function open(page: Page, sim: SimHandle, section = "presets"): Promise<void> {
  await sim.post("/__sim/tv", { key: "system.devmode", value: "1" });
  await expect.poll(async () => (await (await page.request.get("/api/settings")).json())["system.devmode"]).toBe(1);
  await page.addInitScript(() => localStorage.setItem("gbsc.mode", '"advanced"'));
  await page.goto(`/#/dev/${section}`);
  await expect(page.locator(".ui[data-ready]")).toBeVisible();
}
const sec = (page: Page) => page.locator('[data-section="presets"]');
const posts = async (sim: SimHandle, path: string) =>
  (await sim.requests()).requests.filter((r) => r.method === "POST" && r.path === path).map((r) => decodeURIComponent(r.body.replace(/\+/g, " ")));
async function importFile(page: Page, kind: "preset" | "macro", name: string, text: string): Promise<void> {
  const chooser = page.waitForEvent("filechooser");
  await sec(page).getByRole("button", { name: `Import ${kind}` }).click();
  await (await chooser).setFiles({ name, mimeType: "application/x-yaml", buffer: Buffer.from(text) });
}
const row = (page: Page, name: string) => sec(page).locator(".rw", { hasText: name }).first();

test("save preset then apply it", async ({ page, sim }) => {
  await open(page, sim);
  await sec(page).getByRole("button", { name: "New preset" }).click();
  const box = page.getByRole("dialog", { name: "New preset" });
  await box.getByRole("textbox", { name: "Name" }).fill("Bright");
  await expect(box.getByRole("radio", { name: /^Only what differs from Default/ })).toBeChecked();
  await box.getByRole("button", { name: "Save" }).click();
  await expect.poll(async () => (await posts(sim, "/api/adv/preset")).length).toBe(1);
  await expect(row(page, "Bright")).toBeVisible();
  await row(page, "Bright").getByRole("button", { name: "Apply Bright" }).click();
  const ask = page.getByRole("dialog", { name: "Apply Bright?" });
  await expect(ask).toContainText("registers");
  await ask.getByRole("button", { name: /^Apply/ }).click();
  await expect.poll(async () => (await posts(sim, "/api/actions/dev.adv_i2c")).length).toBeGreaterThan(0);
  await expect(row(page, "Bright")).toContainText("applied");
});

test("preset apply skips read-only and asks for dangerous registers", async ({ page, sim }) => {
  await open(page, sim);
  await importFile(page, "preset", "risky.yaml", "name: Risky\ndevices:\n  ADV7280:\n    maps:\n      user_sub_map:\n        map_value: '0x00'\n        registers:\n" +
    "          '0x0A': '0x10'\n          '0x0E': '0x40'\n          '0x10': '0x00'\n          '0x0F': '0x80'\n");
  await expect(row(page, "Risky")).toBeVisible();
  await row(page, "Risky").getByRole("button", { name: "Apply Risky" }).click();
  const ask = page.getByRole("dialog", { name: "Apply Risky?" });
  await expect(ask).toContainText("2 registers");
  await expect(ask).toContainText("0x0E");                   // skipped: map select
  await expect(ask).toContainText("0x10");                   // skipped: read-only
  await expect(ask).toContainText("the picture stays wrong until the input changes or the device restarts");
  await ask.getByRole("button", { name: /^Apply/ }).click();
  await expect.poll(() => posts(sim, "/api/actions/dev.adv_i2c")).toEqual(["data=42,0A,10,42,0F,80"]);
});

test("import an adv-manager preset and export it unchanged", async ({ page, sim }) => {
  const text = readFileSync(join(ADV, "presets", "default.yaml"), "utf8");
  await open(page, sim);
  await importFile(page, "preset", "default.yaml", text);
  await expect(sec(page).getByRole("button", { name: "Export Default" })).toBeVisible();
  const dl = page.waitForEvent("download");
  await sec(page).getByRole("button", { name: "Export Default" }).click();
  const file = await dl;
  expect(file.suggestedFilename()).toBe("Default.yaml");
  expect(readFileSync(await file.path(), "utf8")).toBe(text);
});

test("import without a name takes the file name", async ({ page, sim }) => {
  await open(page, sim);
  await importFile(page, "macro", "soft_rf.yaml", "commands:\n  - w 42 0A 10\n");
  await expect(row(page, "soft_rf")).toBeVisible();
  expect((await (await page.request.get("/api/adv/macro?id=m00")).text()).startsWith("name: soft_rf\n")).toBe(true);
});

test("macro built from options runs its steps", async ({ page, sim }) => {
  await open(page, sim);
  await sec(page).getByRole("button", { name: "New macro" }).click();
  const box = page.getByRole("dialog", { name: "New macro" });
  await box.getByRole("textbox", { name: "Name" }).fill("ACE on");
  await box.getByRole("button", { name: "Add an option" }).click();
  await box.getByRole("searchbox", { name: "Search options" }).fill("ACE_ENABLE");
  await box.getByRole("button", { name: /^ACE_ENABLE/ }).click();
  await box.getByRole("radio", { name: "Enable ACE" }).click();
  await box.getByRole("button", { name: "Add step" }).click();
  await expect(box.locator(".stepr")).toHaveCount(1);
  await box.getByRole("button", { name: "Save" }).click();
  await expect(row(page, "ACE on")).toBeVisible();
  await row(page, "ACE on").getByRole("button", { name: "Run ACE on" }).click();
  await expect.poll(() => posts(sim, "/api/actions/dev.adv_i2c")).toEqual([expect.stringMatching(/^data=42,0E,40,42,80,[89A-F][0-9A-F],42,0E,00$/)]);
});

test("macro steps reorder and remove", async ({ page, sim }) => {
  await open(page, sim);
  await importFile(page, "macro", "three.yaml", "name: Three\ncommands:\n  - w 42 0A 10\n  - w 42 08 80\n  - w 56 17 02\n");
  await row(page, "Three").getByRole("button", { name: "Edit Three" }).click();
  const box = page.getByRole("dialog", { name: "Edit macro" });
  await expect(box.locator(".stepr")).toHaveCount(3);
  await box.getByRole("button", { name: "Move step 3 up" }).click();
  await box.getByRole("button", { name: "Remove step 1" }).click();
  await box.getByRole("button", { name: "Save" }).click();
  await expect.poll(async () => (await (await page.request.get("/api/adv/macro?id=m00")).text())).toBe("name: Three\ncommands:\n  - w 56 17 02\n  - w 42 08 80\n");
});

test("record captures an apply", async ({ page, sim }) => {
  await open(page, sim);
  await sec(page).getByRole("button", { name: "New macro" }).click();
  await page.getByRole("dialog", { name: "New macro" }).getByRole("textbox", { name: "Name" }).fill("Brighter");
  await page.getByRole("dialog", { name: "New macro" }).getByRole("button", { name: "Record" }).click();
  await page.goto("/#/dev/decoder");
  const dec = page.locator('[data-section="decoder"]');
  await expect(dec).toContainText("Recording");
  await dec.getByRole("searchbox", { name: "Search options" }).fill("brightness");
  await dec.locator('[data-areg="42:00:0a"]').getByRole("button", { name: /^Increase BRI/ }).click();
  await dec.locator(".pbar").getByRole("button", { name: "Apply" }).click();
  const box = page.getByRole("dialog", { name: "New macro" });
  await expect(box.locator(".stepr")).toHaveCount(1);
  await expect(box.locator(".stepr")).toContainText("Brightness adjust");
  await box.getByRole("button", { name: "Save" }).click();
  await expect.poll(async () => (await (await page.request.get("/api/adv/macro?id=m00")).text())).toMatch(/^name: Brighter\ncommands:\n  - w 42 0A [0-9A-F]{2}\n$/);
});

test("store error shows the reason", async ({ page, sim }) => {
  await open(page, sim);
  await sim.post("/__sim/fault", { name: "advSpace" });
  expectConsoleError(page, /507|Insufficient Storage|status of 507/);   // the refusal is the point of the test
  await sec(page).getByRole("button", { name: "New preset" }).click();
  const box = page.getByRole("dialog", { name: "New preset" });
  await box.getByRole("textbox", { name: "Name" }).fill("Full");
  await box.getByRole("button", { name: "Save" }).click();
  await expect(page.getByText(/not enough space/i)).toBeVisible();
});

test("delete a preset and a macro after asking", async ({ page, sim }) => {
  await open(page, sim);
  await importFile(page, "preset", "p.yaml", "name: Gone\ndevices:\n  ADV7280:\n    maps:\n      user_sub_map:\n        map_value: '0x00'\n        registers:\n          '0x0A': '0x10'\n");
  await expect(row(page, "Gone")).toBeVisible();   // one store job at a time: the next upload waits for this one
  await importFile(page, "macro", "m.yaml", "name: Gone too\ncommands:\n  - w 42 0A 10\n");
  await expect(row(page, "Gone too")).toBeVisible();
  for (const [name, kind] of [["Gone", "preset"], ["Gone too", "macro"]] as const) {
    await row(page, name).getByRole("button", { name: `Edit ${name}` }).click();
    await page.getByRole("dialog", { name: `Edit ${kind}` }).getByRole("button", { name: "Delete" }).click();
    await page.getByRole("dialog", { name: `Delete ${name}?` }).getByRole("button", { name: "Delete" }).click();
    await expect.poll(async () => (await posts(sim, `/api/adv/${kind}/delete`)).length).toBe(1);
  }
  await expect(sec(page).getByRole("button", { name: /^Edit Gone/ })).toHaveCount(0);
});

test("preset apply without a reply is not shown as applied", async ({ page, sim }) => {
  await open(page, sim);
  await sim.post("/__sim/fault", { name: "advNoReply" });
  await row(page, "Default").getByRole("button", { name: "Apply Default" }).click();
  await page.getByRole("dialog", { name: "Apply Default?" }).getByRole("button", { name: /^Apply/ }).click();
  await expect(page.getByText(/not confirmed/)).toBeVisible();
  await expect(row(page, "Default")).not.toContainText("applied");
});

test("new preset is refused when the chips do not answer", async ({ page, sim }) => {
  await open(page, sim);
  await sim.post("/__sim/fault", { name: "advNoReply" });
  await sec(page).getByRole("button", { name: "New preset" }).click();
  const box = page.getByRole("dialog", { name: "New preset" });
  await box.getByRole("textbox", { name: "Name" }).fill("Stale");
  await box.getByRole("button", { name: "Save" }).click();
  await expect(page.getByText(/did not answer/)).toBeVisible();
  expect((await sim.requests()).requests.filter((r) => r.method === "POST" && r.path === "/api/adv/preset")).toEqual([]);
});

test("presets above macros, built-in macros run what the firmware applies", async ({ page, sim }) => {
  await open(page, sim);
  const presets = sec(page).locator(".lhead", { hasText: "Presets" }), macros = sec(page).locator(".lhead", { hasText: "Macros" });
  const p = (await presets.boundingBox())!, m = (await macros.boundingBox())!;
  expect(m.y).toBeGreaterThan(p.y + p.height);                 // one under the other
  expect(Math.abs(m.x - p.x)).toBeLessThan(2);
  const ace = row(page, "Enable ACE");
  await expect(ace).toContainText("built-in");
  await expect(ace.getByRole("button", { name: "Edit Enable ACE" })).toHaveCount(0);
  await expect(row(page, "Enable I2P 2X")).toBeVisible();
  await ace.getByRole("button", { name: "Run Enable ACE" }).click();
  await expect.poll(() => posts(sim, "/api/actions/dev.adv_i2c")).toEqual(["data=42,0E,40,42,80,80,42,0E,00"]);
});

test("a save whose result never arrives is checked in the list", async ({ page, sim }) => {
  await open(page, sim);
  await sim.post("/__sim/fault", { name: "wsSilent" });   // the socket stays open but says nothing
  await importFile(page, "macro", "late.yaml", "name: Late\ncommands:\n  - w 42 0A 10\n");
  await expect(row(page, "Late")).toBeVisible({ timeout: 15000 });
  await expect(page.getByText(/Not saved/)).toHaveCount(0);
});

test("a full action queue is waited out, not the end of the run", async ({ page, sim }) => {
  await open(page, sim);
  expectConsoleError(page, /409|Conflict/);                     // the refusals before the queue frees up
  await sim.post("/__sim/fault", { name: "queueFull" });
  setTimeout(() => void sim.post("/__sim/fault", { name: "queueFull", on: false }), 700);
  await row(page, "Enable I2P 2X").getByRole("button", { name: "Run Enable I2P 2X" }).click();
  await expect(page.getByText(/Ran Enable I2P 2X/)).toBeVisible({ timeout: 10000 });
  await expect(page.getByText(/Not sent/)).toHaveCount(0);
});

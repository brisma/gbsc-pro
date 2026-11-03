// Every row of the sectioned pages, generated from layout/pages.ts: change the row → the exact POST; a change made on
// the TV → the row shows it, the section keeps its element (no re-mount). Advanced on phone-pixel8 and
// desktop-chromium-1280, in the first scenario where the row is visible and enabled.
import { test, expect } from "./fixtures.ts";
import type { Page } from "@playwright/test";
import type { SimHandle } from "./fixtures.ts";
import { PAGES } from "../../app/layout/pages.ts";
import type { Row, Group, Section, PageId } from "../../app/layout/types.ts";
import { rowState, rowNode, rowSetting, rowAction, rowLabel } from "../../app/rows/resolve.ts";
import { ITEMS, FN_ACTIONS, type Item } from "./rows.ts";
import { stateFromStore, type PredState } from "../../app/model/predicates.ts";
import { optionsFor, registry, menu } from "../../app/model/menu.ts";
import { choiceValue, choiceWrites, choiceOptions } from "../../app/model/choices.ts";
import { scanlineOptions, scanlineValue, scanlineWrites } from "../../app/model/scanlines.ts";
import { formatSetting } from "../../app/model/format.ts";

export const EVERY_ROW_PROJECTS = ["phone-pixel8", "desktop-chromium-1280"];
const SCENARIOS = ["pro-rgbs", "pro-svideo-ntsc", "pro-composite-ntsc", "pro-composite-pal", "pro-vga", "pro-component", "standard-rgbs"];
const isDesk = (page: Page) => (page.viewportSize()?.width ?? 0) >= 960;
const posts = async (sim: SimHandle, path: string) =>
  (await sim.requests()).requests.filter((r) => r.method === "POST" && r.path === path).map((r) => r.body);
const getJson = async (sim: SimHandle, path: string) => (await fetch(sim.url + path, { headers: { Host: "gbscontrol.local" } })).json();

// A row greyed by another setting (menuEnabled): the setting a user turns on first, as the TV reason says
const DEPENDS: Record<string, Record<string, number>> = {
  ftl_on: { "output.frame_lock": 1 }, ftl_ok: { "output.force_60hz": 0 }, i2p_on: { "adv.i2p": 1 }, deint_ok: { "adv.i2p": 0 },
  ace_on: { "adv.ace": 1 }, sv_wy: { "adv.filter_wy_override": 1 },
};

async function findScenario(sim: SimHandle, it: Item): Promise<{ state: PredState; values: Record<string, number> } | null> {
  for (const sc of SCENARIOS) {
    await sim.post("/__sim/scenario", { name: sc });
    if (it.page === "dev") {   // the Developer page lives in Dev; the device applies the change in its loop
      await sim.post("/__sim/tv", { key: "system.devmode", value: "1" });
      await expect.poll(async () => (await getJson(sim, "/api/settings"))["system.devmode"]).toBe(1);
    }
    const [info, values, wifi] = await Promise.all([getJson(sim, "/api/info"), getJson(sim, "/api/settings"), getJson(sim, "/api/wifi")]);
    let state = stateFromStore({ values, info, wifiMode: wifi.mode, mode: "advanced", profileActive: false });
    let st = rowState(it.row, state, it.group, it.section);
    const dep = DEPENDS[rowNode(it.row)?.pred ?? ""];
    if (st.visible && st.reason && dep) {
      for (const [k, v] of Object.entries(dep)) await sim.post("/__sim/tv", { key: k, value: String(v) });
      await new Promise((ok) => setTimeout(ok, 250));
      const v2 = await getJson(sim, "/api/settings");
      state = stateFromStore({ values: v2, info, wifiMode: wifi.mode, mode: "advanced", profileActive: false });
      st = rowState(it.row, state, it.group, it.section);
      if (st.visible && !st.reason) return { state, values: v2 };
    }
    if (st.visible && !st.reason && !(it.row.phoneOnly)) return { state, values };
  }
  return null;
}

for (const it of ITEMS) {
  test(it.title, async ({ page, sim }, info) => {
    test.skip(!EVERY_ROW_PROJECTS.includes(info.project.name), "every-row runs on phone-pixel8 and desktop-chromium-1280");
    const found = await findScenario(sim, it);
    expect(found, `no scenario shows ${it.title} enabled`).not.toBeNull();
    const { state, values } = found!;
    await page.addInitScript(() => localStorage.setItem("gbsc.mode", '"advanced"'));
    await page.goto(`/#/${it.page}/${it.section.id}`);
    await expect(page.locator(".ui[data-ready]")).toBeVisible();
    const section = page.locator(`[data-section="${it.section.id}"]`);
    const renderId = await section.getAttribute("data-render-id");
    const r = it.row, n = rowNode(r), label = rowLabel(r), key = rowSetting(r);
    const el = page.locator(`[data-row="${r.kind}:${r.ref}"]`).first();
    const desk = isDesk(page);
    const strip = r.kind === "picker" && desk;   // PC: the choices as a strip of cards in the row
    await expect(el).toBeVisible();
    const before = (await sim.requests()).requests.length;
    const newPosts = async (path: string) => (await sim.requests()).requests.slice(before).filter((x) => x.method === "POST" && x.path === path).map((x) => x.body);
    const num = (k: string) => Number(values[k] ?? 0);

    // ---- change the row ----
    let expected: { path: string; body: string };
    let settled: number | null = null;   // the setting's value after the change (for the TV step)
    if (n?.type === "choice" || r.kind === "picker" || (r.kind === "cards" && !key)) {
      const choice = n?.choice;
      if (choice === "scanlines") {
        const cur = scanlineValue("advanced", values);
        const o = scanlineOptions("advanced").find((x) => x.label !== cur && x.label !== "Custom")!;
        expected = { path: "/api/settings", body: scanlineWrites("advanced", o.label).map(([k, v]) => `${k}=${v}`).join("&") };
        await page.getByRole("radiogroup", { name: label }).getByRole("radio", { name: new RegExp(`^${o.label}\\b`) }).click();
      } else if (choice === "filter") {
        const cur = choiceValue("filter", values);
        const o = choiceOptions("filter", { mode: "advanced", caps: state.caps, current: cur }).find((x) => x.index !== cur)!;
        expected = { path: "/api/settings", body: choiceWrites("filter", o.index).map(([k, v]) => `${k}=${v}`).join("&") };
        await page.getByRole("radiogroup", { name: label }).getByRole("radio", { name: new RegExp(`^${o.label}\\b`) }).click();
      } else if (choice === "resolution") {
        const cur = choiceValue("resolution", values);
        const o = choiceOptions("resolution", { mode: "advanced", caps: state.caps, current: cur }).find((x) => x.index !== cur)!;
        expected = { path: "/api/settings", body: choiceWrites("resolution", o.index).map(([k, v]) => `${k}=${v}`).join("&") };
        const name = new RegExp(`^${o.label.replace(/[/]/g, "\\/")}`);
        if (strip) await el.getByRole("radiogroup", { name: "Resolution" }).getByRole("radio", { name }).click();
        else { await el.getByRole("button", { name: new RegExp(`^${label}:`) }).click(); await page.getByRole("listbox", { name: "Resolution" }).getByRole("option", { name }).click(); }
      } else {   // the source picker
        const opts = optionsFor(n!, { mode: "advanced", caps: state.caps, current: num("input.source"), profileActive: false }).filter((o) => o.v > 0 && o.v !== num("input.source"));
        expected = { path: "/api/settings", body: `input.source=${opts[0].v}` };
        if (strip) await el.getByRole("radiogroup", { name: "Source" }).getByRole("radio", { name: opts[0].n, exact: true }).click();
        else { await el.getByRole("button", { name: /^Source:/ }).click(); await page.getByRole("listbox", { name: "Source" }).getByRole("option", { name: new RegExp(`^${opts[0].n}`) }).click(); }
      }
    } else if (n?.type === "toggle_fn") {
      expected = { path: "/api/actions/input.termination", body: "data=1" };
      await el.getByRole("switch").click();
    } else if (n?.type === "adjust2d") {
      expected = { path: `/api/actions/${menu.adjust[n.adjust!].up}`, body: "" };
      await el.getByRole("button", { name: `${label} up` }).click();   // the arrows sit in the row
    } else if (n?.type === "stepper") {
      expected = { path: `/api/actions/${n.action_inc}`, body: "" };
      await el.getByRole("button", { name: `Increase ${label}` }).click();
    } else if (r.kind === "toggle" && n?.type === "action") {   // a developer toggle: the action, the readback follows
      expected = { path: `/api/actions/${n.action}`, body: "" };
      await el.getByRole("switch").click();
    } else if (r.kind === "action") {
      const name = (n?.fn ? FN_ACTIONS[n.fn] : undefined) ?? rowAction(r)!;
      expected = { path: `/api/actions/${name}`, body: "" };
      if (r.head) await el.click();   // a button of the card header: the element is the button
      else await el.getByRole("button").last().click();
      const dialog = page.getByRole("dialog");
      if (await dialog.count()) await dialog.locator(".btn.pri").click();
    } else if (r.kind === "toggle") {
      settled = num(key!) ? 0 : 1;
      expected = { path: "/api/settings", body: `${key}=${settled}` };
      await el.getByRole("switch").click();
    } else if (r.kind === "stepper") {
      const e = registry.entry(key!)!, step = n?.step ?? 1, v = num(key!);
      settled = v + step <= (e.max ?? 255) ? v + step : v - step;
      expected = { path: "/api/settings", body: `${key}=${settled}` };
      await el.getByRole("button", { name: `${settled > v ? "Increase" : "Decrease"} ${label}` }).click();
    } else {   // inline, menu, setting cards
      const opts = optionsFor(n!, { mode: "advanced", caps: state.caps, current: num(key!), profileActive: false });
      const o = opts.find((x) => x.v !== num(key!))!;
      settled = o.v;
      expected = { path: "/api/settings", body: `${key}=${o.v}` };
      if (r.kind === "cards") {
        await page.getByRole("radiogroup", { name: label }).getByRole("radio", { name: new RegExp(`^${o.n}\\b`) }).click();
      } else if (await el.getByRole("radiogroup").count()) {
        await el.getByRole("radio", { name: o.n, exact: true }).click();
      } else {
        await el.getByRole("button", { name: new RegExp(`^${label}:`) }).click();
        await page.getByRole("listbox", { name: label }).getByRole("option", { name: o.n, exact: true }).click();
      }
    }
    await expect.poll(() => newPosts(expected.path), { message: `POST ${expected.path}` }).toEqual([expected.body]);

    // ---- the same setting changed on the TV ----
    if (settled !== null && key) {
      const e = registry.entry(key)!;
      let v2: number;
      if (r.kind === "toggle") v2 = settled ? 0 : 1;
      else if (r.kind === "stepper") { const step = n?.step ?? 1; v2 = settled + step <= (e.max ?? 255) ? settled + step : settled - step; }
      else v2 = optionsFor(n!, { mode: "advanced", caps: state.caps, current: settled, profileActive: false }).find((x) => x.v !== settled)!.v;
      await sim.post("/__sim/tv", { key, value: String(v2) });
      const shown = n ? formatSetting(n, v2) : String(v2);
      if (r.kind === "toggle") await expect(el.getByRole("switch")).toHaveAttribute("aria-checked", String(v2 === 1));
      else if (r.kind === "stepper") await expect(el.getByRole("button", { name: new RegExp(`^${label} ${shown.replace(/[%]/g, "\\%")}$`) })).toBeVisible();
      else if (r.kind === "cards") await expect(page.getByRole("radiogroup", { name: label }).getByRole("radio", { name: new RegExp(`^${shown}\\b`) })).toHaveAttribute("aria-checked", "true");
      else if (await el.getByRole("radiogroup").count()) await expect(el.getByRole("radio", { name: shown, exact: true })).toHaveAttribute("aria-checked", "true");
      else await expect(el.getByRole("button", { name: new RegExp(`^${label}:`) })).toHaveAccessibleName(`${label}: ${shown}`);
      expect(await section.getAttribute("data-render-id"), "the section was re-mounted").toBe(renderId);
    }
  });
}

// Easy: visibility only — the rows on screen are exactly the ones the predicates show in Easy
const EASY: Record<string, string[]> = { settings: ["pro-rgbs", "pro-svideo-ntsc"], system: ["pro-rgbs", "standard-rgbs"] };
for (const p of PAGES.filter((x) => EASY[x.id])) for (const sec of p.sections.filter((x) => x.groups)) for (const sc of EASY[p.id])
  test(`every-row easy ${p.id}/${sec.id} ${sc}`, async ({ page, sim }, info) => {
    test.skip(!EVERY_ROW_PROJECTS.includes(info.project.name), "every-row runs on phone-pixel8 and desktop-chromium-1280");
    await sim.post("/__sim/scenario", { name: sc });
    const [inf, values, wifi] = await Promise.all([getJson(sim, "/api/info"), getJson(sim, "/api/settings"), getJson(sim, "/api/wifi")]);
    const state = stateFromStore({ values, info: inf, wifiMode: wifi.mode, mode: "easy", profileActive: false });
    await page.addInitScript(() => localStorage.setItem("gbsc.mode", '"easy"'));
    await page.goto(`/#/${p.id}/${sec.id}`);
    await expect(page.locator(".ui[data-ready]")).toBeVisible();
    const desk = isDesk(page);
    const want = (sec.groups ?? []).flatMap((g) => g.rows.filter((r) => r.kind !== "note" && rowState(r, state, g, sec).visible &&
      !(r.phoneOnly && desk)).map((r) => `${r.kind}:${r.ref ?? r.label}`));
    const section = page.locator(`[data-section="${sec.id}"]`);
    if (!want.length) { await expect(section, "a section with no row here is left out").toHaveCount(0); return; }
    await expect(section.locator("[data-row]").first()).toBeVisible();
    const got = await section.locator(":scope [data-row]").evaluateAll((els) => els.filter((e) => !e.closest(".inl")).map((e) => e.getAttribute("data-row")));
    expect([...got].sort()).toEqual([...want].sort());   // PC columns may reorder whole groups
  });

test("every-row covers the settings pages", () => {
  expect(ITEMS.length).toBeGreaterThan(40);
});

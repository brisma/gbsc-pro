// Layout, accessibility and speed checks with a pass/fail rule
import type { Page } from "@playwright/test";
import AxeBuilder from "@axe-core/playwright";
import type { SimHandle } from "./fixtures.ts";

const INTERACTIVE = "button, a[href], input, select, textarea, [role=button], [role=switch], [role=tab]";

// No element wider than its container, unless the container scrolls or clips, or the element is marked data-truncate
export async function expectNoOverflow(page: Page): Promise<void> {
  const bad = await page.evaluate(() => {
    const out: string[] = [];
    if (document.documentElement.scrollWidth > window.innerWidth + 1) out.push(`the page scrolls sideways (${document.documentElement.scrollWidth} > ${window.innerWidth})`);
    for (const el of Array.from(document.body.querySelectorAll<HTMLElement>("*"))) {
      const p = el.parentElement;
      if (!p || p === document.body || el.closest("[data-truncate]")) continue;
      const ps = getComputedStyle(p);
      if (ps.overflowX !== "visible" || ps.display === "contents") continue;
      const r = el.getBoundingClientRect(), pr = p.getBoundingClientRect();
      if (r.width && r.right > pr.right + 1) out.push(`${el.tagName.toLowerCase()}${el.id ? "#" + el.id : ""}${el.className && typeof el.className === "string" ? "." + el.className.split(" ")[0] : ""} "${(el.textContent || el.getAttribute("aria-label") || "").trim().slice(0, 30)}" is wider than its container (${p.tagName.toLowerCase()}.${String(p.className).split(" ")[0]}, ${Math.round(r.right - pr.right)} px)`);
    }
    return out;
  });
  if (bad.length) throw new Error(bad.slice(0, 10).join("\n"));
}

// Every visible interactive element, scrolled to the centre of its scroll area (as far as it goes), has its centre on
// itself: nothing stays on top of it (tab bar, header, sheets). Something under a fixed bar only until the page
// scrolls is fine; something the page cannot scroll out from under it is not. Scroll positions are restored.
export async function expectNotCovered(page: Page, selector = INTERACTIVE): Promise<void> {
  const bad = await page.evaluate((sel) => {
    const out: string[] = [];
    const scrolled = Array.from(document.querySelectorAll<HTMLElement>("*")).filter((e) => e.scrollHeight > e.clientHeight || e.scrollWidth > e.clientWidth).map((e) => [e, e.scrollTop, e.scrollLeft] as const);
    const winY = scrollY, winX = scrollX;
    for (const el of Array.from(document.querySelectorAll<HTMLElement>(sel))) {
      let r = el.getBoundingClientRect();
      if (!r.width || !r.height || getComputedStyle(el).visibility === "hidden" || el.matches(":disabled")) continue;   // a disabled control takes no tap
      el.scrollIntoView({ block: "center", inline: "nearest", behavior: "instant" as ScrollBehavior });
      r = el.getBoundingClientRect();
      const x = r.left + r.width / 2, y = r.top + r.height / 2;
      if (x < 0 || y < 0 || x > innerWidth || y > innerHeight) continue;   // in a clipped area the page never shows
      const hit = document.elementFromPoint(x, y);
      if (hit && hit !== el && !el.contains(hit)) out.push(`${el.tagName.toLowerCase()} "${(el.textContent || el.getAttribute("aria-label") || "").trim().slice(0, 30)}" is covered by ${hit.tagName.toLowerCase()}${hit.className ? "." + String(hit.className).split(" ")[0] : ""}`);
    }
    for (const [e, t, l] of scrolled) { e.scrollTop = t; e.scrollLeft = l; }
    scrollTo(winX, winY);
    return out;
  }, selector);
  if (bad.length) throw new Error(bad.slice(0, 10).join("\n"));
}

// Tap targets of at least min x min CSS px on phones (axe checks only 24 px). The tap area is what a finger hits, not
// the drawing: a control may be drawn smaller and reach the size with transparent borders or a pseudo-element (also to
// one side only, next to a neighbour), so each control is brought into view and its area is walked from the centre,
// across and down. Scroll positions are restored.
export async function expectTapTargets(page: Page, min = 44): Promise<void> {
  const bad = await page.evaluate(([sel, m]) => {
    const out: string[] = [];
    const need = (m as number) - 1;
    const scrolled = Array.from(document.querySelectorAll<HTMLElement>("*")).filter((e) => e.scrollHeight > e.clientHeight || e.scrollWidth > e.clientWidth).map((e) => [e, e.scrollTop, e.scrollLeft] as const);
    const winY = scrollY, winX = scrollX;
    for (const el of Array.from(document.querySelectorAll<HTMLElement>(sel as string))) {
      let r = el.getBoundingClientRect();
      if (!r.width || !r.height || getComputedStyle(el).visibility === "hidden" || el.matches(":disabled")) continue;
      if (r.width >= (m as number) && r.height >= (m as number)) continue;   // the box itself is large enough
      el.scrollIntoView({ block: "center", inline: "nearest", behavior: "instant" as ScrollBehavior });
      r = el.getBoundingClientRect();
      const x = r.left + r.width / 2, y = r.top + r.height / 2;
      const mine = (px: number, py: number) => { const h = document.elementFromPoint(px, py); return !!h && (h === el || el.contains(h)); };
      if (x < 0 || y < 0 || x > innerWidth || y > innerHeight || !mine(x, y)) continue;   // clipped or covered: expectNotCovered's subject
      const reach = (dx: number, dy: number) => { let n = 0; while (n < (m as number) && mine(x + dx * (n + 1), y + dy * (n + 1))) n++; return n; };
      const w = reach(-1, 0) + reach(1, 0) + 1, h = reach(0, -1) + reach(0, 1) + 1;
      if (w < need || h < need) out.push(`${el.tagName.toLowerCase()}${el.className && typeof el.className === "string" ? "." + el.className.split(" ")[0] : ""} "${(el.textContent || el.getAttribute("aria-label") || "").trim().slice(0, 30)}" is drawn ${Math.round(r.width)}x${Math.round(r.height)}, its tap area is ${w}x${h}`);
    }
    for (const [e, t, l] of scrolled) { e.scrollTop = t; e.scrollLeft = l; }
    scrollTo(winX, winY);
    return out;
  }, [INTERACTIVE, min]);
  if (bad.length) throw new Error(`tap targets under ${min} px:\n` + bad.slice(0, 40).join("\n"));
}

export async function expectA11y(page: Page): Promise<void> {
  const r = await new AxeBuilder({ page }).withTags(["wcag2a", "wcag2aa"]).analyze();
  if (r.violations.length)
    throw new Error(r.violations.map((v) => `${v.id}: ${v.help} (${v.nodes.length}): ` +
      v.nodes.slice(0, 3).map((n) => `${n.target.join(" ")} ${n.any[0]?.message ?? ""}`.trim()).join(" | ")).join("\n"));
}

// Chromium: CPU x4 and the device network profile; ms from navigation until selector is visible
// First usable screen with the device network profile and a 4x slower CPU. On the device the WiFi moves ~25 KB/s even
// for the static page, so the HTML alone takes about 1 s per 25 KB: the check times the part the app controls, from the
// page parsed (DOMContentLoaded) to the selector; the total is returned too.
export async function measureFirstUsable(page: Page, sim: SimHandle, selector: string): Promise<{ afterPage: number; total: number }> {
  const cdp = await page.context().newCDPSession(page);
  await cdp.send("Emulation.setCPUThrottlingRate", { rate: 4 });
  if (sim.sim) await sim.post("/__sim/net", { profile: "device" });
  const t0 = Date.now();
  await page.goto("/", { waitUntil: "domcontentloaded" });
  const t1 = Date.now();
  await page.locator(selector).first().waitFor({ state: "visible" });
  const t2 = Date.now();
  await cdp.send("Emulation.setCPUThrottlingRate", { rate: 1 });
  return { afterPage: t2 - t1, total: t2 - t0 };
}

// Fixtures of the harness: a simulator per worker, a fresh scenario per test, a page that fails on console errors, an
// offline guard for HTTP and WebSocket, and the request discipline check after each test
import { test as base, expect, type Page } from "@playwright/test";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { startSim, type Sim } from "../../sim/server.ts";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");

export interface SimHandle {
  sim: Sim | null; url: string;
  post(path: string, body: unknown): Promise<void>;
  requests(): Promise<{ requests: { method: string; path: string; body: string }[]; maxInFlight: number; duringUpload: number }>;
}

const allowed = new WeakMap<Page, RegExp[]>();
// Allows console errors and failed requests matching re ("<status> <url>" or "failed <url>") for this test
export function expectConsoleError(page: Page, re: RegExp): void {
  allowed.set(page, [...(allowed.get(page) || []), re]);
}

// A test that navigates again (reload, a second goto) aborts the polls in flight: Chromium and Firefox report a
// failed request, WebKit logs "Fetch API cannot load … due to access control checks"
export function allowNavigationAborts(page: Page): void {
  expectConsoleError(page, /^failed |due to access control checks/);
}

export const test = base.extend<{ sim: SimHandle; guard: { blocked: string[] }; scenarioReset: void }, { simWorker: SimHandle }>({
  simWorker: [async ({}, use, workerInfo) => {
    const device = workerInfo.project.name === "device" ? process.env.GBSC_DEVICE : undefined;   // no simulator then
    if (device) {
      await use({ sim: null, url: `http://${device}`, async post() { throw new Error("no /__sim on a device"); },
                  async requests() { return { requests: [], maxInFlight: 0, duringUpload: 0 }; } });
      return;
    }
    const sim = await startSim({ port: 0, wsPort: 0, pagePath: join(root, "dist", "app.html") });
    const handle: SimHandle = {
      sim, url: sim.url,
      async post(path, body) {
        const r = await fetch(sim.url + path, { method: "POST", body: JSON.stringify(body), headers: { "content-type": "application/json" } });
        if (!r.ok) throw new Error(`${path}: ${r.status}`);
      },
      async requests() { return (await fetch(sim.url + "/__sim/requests")).json(); },
    };
    await use(handle);
    await sim.stop();
  }, { scope: "worker" }],

  sim: async ({ simWorker }, use) => { await use(simWorker); },

  scenarioReset: [async ({ simWorker }, use) => {
    if (simWorker.sim) await simWorker.post("/__sim/scenario", { name: "pro-rgbs" });
    await use();
    if (simWorker.sim) {
      const log = await simWorker.requests();
      expect(log.maxInFlight, "the page kept more than 2 requests in flight (device discipline)").toBeLessThanOrEqual(2);
      expect(log.duringUpload, "requests were sent while an upload ran").toBe(0);
    }
  }, { auto: true }],

  baseURL: async ({ simWorker }, use) => { await use(simWorker.url); },

  guard: async ({}, use) => { await use({ blocked: [] }); },

  page: async ({ page, simWorker, guard }, use) => {
    const origin = new URL(simWorker.url).origin;
    const wsOrigin = simWorker.sim ? `ws://127.0.0.1:${simWorker.sim.wsPort}` : `ws://${new URL(simWorker.url).hostname}:81`;
    await page.route("**/*", (route) => {
      const u = route.request().url();
      if (u.startsWith(origin) || u.startsWith("data:") || u.startsWith("blob:")) return route.continue();
      guard.blocked.push(u);
      return route.abort("blockedbyclient");
    });
    await page.routeWebSocket(/.*/, (ws) => {
      if (ws.url().startsWith(wsOrigin)) { ws.connectToServer(); return; }
      guard.blocked.push(ws.url());
      ws.close();
    });
    const errors: string[] = [];
    // A request in flight when the test itself loads the page again is cancelled by the browser, and the closing
    // page's retry is refused (WebKit: "Fetch API cannot load ... due to access control checks", as a page error or a
    // console line). Forgiven only with a new document within a second of it
    const cancelled: { text: string; at: number }[] = [];
    const documents: number[] = [];
    const note = (text: string, unload: boolean) => { if (unload) cancelled.push({ text, at: Date.now() }); else errors.push(text); };
    const refused = /due to access control checks/;
    page.on("domcontentloaded", () => documents.push(Date.now()));
    page.on("console", (m) => { if (m.type() === "error") note(m.text(), refused.test(m.text())); });
    page.on("pageerror", (e) => note(String(e), refused.test(String(e))));
    // every engine: an HTTP error or a failed request to the device (Firefox logs no console error for them)
    page.on("response", (r) => { if (r.status() >= 400 && r.url().startsWith(origin)) errors.push(`${r.status()} ${r.url()}`); });
    page.on("requestfailed", (r) => {
      if (r.url().startsWith(origin)) note(`failed ${r.url()}`, /abort|cancel/i.test(r.failure()?.errorText ?? ""));
    });
    await use(page);
    for (const c of cancelled)
      if (!documents.some((d) => Math.abs(d - c.at) < 1000)) errors.push(c.text);
    const ok = allowed.get(page) || [];
    const bad = errors.filter((e) => !ok.some((re) => re.test(e)));
    expect(bad, "console errors on the page").toEqual([]);
    expect(guard.blocked, "requests outside the device").toEqual([]);
  },
});

export { expect };

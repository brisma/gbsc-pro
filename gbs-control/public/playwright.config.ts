// Playwright harness: the projects below; a simulator per worker (test/e2e/fixtures.ts)
import { defineConfig, devices } from "@playwright/test";

const desktop = (name: string, device: string, width: number, height: number) =>
  ({ name, use: { ...devices[device], viewport: { width, height } } });

// WebKit on a macOS host leaks kernel memory (~50 MB per page that is never given back, measured on
// macOS 27.0.1 with Playwright WebKit 2370; a full run filled the compressor with 40-50 GB and jetsam killed the
// system daemons): on macOS the WebKit projects are left out; they run in the container and in CI
export const WEBKIT_PROJECTS = ["phone-iphone15", "tablet", "desktop-webkit-1280"];
// The quick run: an Android phone, an iPhone and a PC; GBSC_E2E=full runs every project
// (tools/dev.py check --full, CI)
export const QUICK_PROJECTS = ["phone-pixel8", "phone-iphone15", "desktop-chromium-1280"];
const full = process.env.GBSC_E2E === "full";
const macHost = process.platform === "darwin";

export default defineConfig({
  testDir: "test/e2e",
  globalSetup: "./test/e2e/global-setup.ts",
  fullyParallel: true,
  // 3 browsers at a time locally (a 16 GB laptop ran out of memory with 5); GBSC_PW_WORKERS overrides
  workers: Number(process.env.GBSC_PW_WORKERS) || 3,
  forbidOnly: !!process.env.CI,
  retries: 0,
  reporter: [["list"], ["html", { open: "never" }]],
  // the app honours prefers-reduced-motion: no sheet is clicked while it slides in (animations are not under test)
  use: { trace: "retain-on-failure", reducedMotion: "reduce" },
  projects: [
    { name: "phone-iphone15", use: { ...devices["iPhone 15"] }, metadata: { phone: true } },
    { name: "phone-pixel8", use: { ...devices["Pixel 8"] }, metadata: { phone: true } },
    { name: "phone-small", use: { ...devices["Pixel 8"], viewport: { width: 360, height: 800 }, deviceScaleFactor: 2 }, metadata: { phone: true } },
    { name: "tablet", use: { ...devices["iPad Mini"] } },
    desktop("desktop-chromium-1280", "Desktop Chrome", 1280, 800),
    desktop("desktop-firefox-1280", "Desktop Firefox", 1280, 800),
    desktop("desktop-webkit-1280", "Desktop Safari", 1280, 800),
    desktop("desktop-chromium-1920", "Desktop Chrome", 1920, 1080),
    // A real device: GBSC_DEVICE=<ip> npx playwright test --project device (only @device tests, read-only)
    ...(process.env.GBSC_DEVICE ? [{ name: "device", grep: /@device/, use: { ...devices["Desktop Chrome"] } }] : []),
  ].filter((p) => !(macHost && WEBKIT_PROJECTS.includes(p.name)))
   .filter((p) => full || p.name === "device" || QUICK_PROJECTS.includes(p.name)),
});

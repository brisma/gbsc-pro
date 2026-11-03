# GBS-Control web app

The web app of the GBSC PRO: one page (TypeScript, Preact with signals, built by esbuild) that GBS-Control serves at
`http://gbscontrol.local/` (or the unit's IP). It shows the same settings as the TV menu, profiles, status and the
console log, WiFi, updates, backup and restore, a remote for the TV menu and, in Developer mode, register tools for
the scaler and the ADV Controller.

The app hardcodes none of the firmware's tables: settings, actions, routes and the menu tree are exported from the
firmware sources into `gen/` and embedded at build time. A Node.js simulator answers like the device, so the app is
developed and tested without hardware.

- Installing the tools, `tools/dev.py` and the container: [CONTRIBUTING](../../CONTRIBUTING.md).
- The firmware side (API routes, settings registry, menu tree): [GBS-Control](../README.md).

## Quick start

```bash
npm ci
npm run gen          # firmware tables -> gen/ (needs PlatformIO with a host C++ compiler and Python with PyYAML)
npm run build        # dist/app.html
npm run sim          # the simulator: page on http://127.0.0.1:8080/, WebSocket on :8081
npm run check        # every check below, Playwright quick run included
```

From the repository root, `python tools/dev.py check web` runs the same checks (`--full` for every browser).

## How the firmware serves it

`pio run` in `gbs-control/` builds the page when it is missing or older than its sources (pre-script
`tools/webui-build.py`): `npm run gen` when `gen/` is stale, `npm run build:app`, then `webui_html.h`, the page
gzipped, which the firmware embeds. `GET /` answers it with `Content-Encoding: gzip`, `Cache-Control: no-cache` and
the build id as ETag, so a firmware update never leaves a stale page. `gen/`, `dist/` and `webui_html.h` are not in
git. In safe mode the firmware serves a small built-in page instead.

The page talks to the device through:
- `/api/*` (see [GBS-Control](../README.md#web-api)); every POST carries `X-GBSC: 1`;
- the WebSocket `ws://<host>:81/` (subprotocol `arduino`): `s:<key>=<value>` for settings and status, `l:` for log
  lines, `e:` for history events.

## Layout of `app/`

| Folder | What it holds |
|--------|---------------|
| `main.tsx`, `index.html.tpl` | Entry point and the page template (inline manifest and icon for Add to Home Screen). |
| `shell/` | The frame: `App.tsx`, header, phone tabs and PC sidebar, hash router, sheets, confirmations, toasts, the remote. |
| `layout/` | `pages.ts`: the pages, their sections, groups and rows, as data shared by the phone and PC layouts; `bindings.ts`: where each menu node lives in the app. |
| `rows/` | Row components (toggle, stepper, inline choice, cards, picker, pad, action, ...) and `resolve.ts`, which links a row to its menu node, setting or action. |
| `sections/` | The sectioned pages (Settings, System, Developer). |
| `pages/` | Quick, Profiles, Status, the System sections with their own layout (WiFi, Update, Backup) and the Developer register tools. |
| `store/` | `device.ts`: the device store (settings with pending writes, info, live readbacks, profiles, log, events, connection); `app.ts`: the single store instance and the page state; `persist.ts`: per-browser preferences; `profiles.ts`: profile helpers. |
| `api/` | `client.ts`: HTTP requests, at most 2 in flight, busy `503` retried; `ws.ts`: the WebSocket with dead-link detection and reconnect. |
| `model/` | Firmware logic ported or read from `gen/`: menu tree, registry, visibility predicates, value formatting, choices, scanlines, version order, backup checks, zip. |
| `regs/` | Developer register tools: register map, TV5725 field writes, ADV write plans, the ADV Manager YAML subset, the ADV Controller console syntax. |
| `texts/` | UI strings, help texts, event and update words. |
| `art/`, `styles/` | Icons, logo, connector art, CSS tokens and base styles. |
| `dev/components.tsx` | A page with every row component (served by the simulator at `/__sim/components`). |

### Pages

Hash routes (`shell/router.ts`): `#/quick`, `#/settings/<section>`, `#/profiles[/<id>]`, `#/status/<section>`,
`#/system/<section>`, `#/dev/<section>`.

| Page | Sections |
|------|----------|
| Quick | Signal chain and pinned controls (pins kept per browser) |
| Settings | Input, Output, Picture, Screen (rows in the TV menu's order) |
| Profiles | List, load, save, manage, export and import |
| Status | Console, Signal (with the history), Diagnostics |
| System | WiFi, Device, TV and OLED menu, Update, Backup, Restart and reset |
| Developer | Sync, Clock / ADC, Debug, Registers (TV5725); Decoder, Encoder, Presets and macros, Manual and console (ADV Controller); Serial log, Arduino IDE upload |

Easy and Advanced is a per-browser choice; the Developer page needs Advanced and `system.devmode` on the device.
Sections, rows and options whose hardware is missing (`cap` in `layout/pages.ts`, capabilities in the registry) are
hidden. Below 960 px the app uses the phone layout (header, tabs at the bottom), above it the PC layout (sidebar).

### Firmware tables (`gen/`)

`npm run gen` (`tools/gen.mjs`) runs the native exporter `test/test_web_export` of the firmware
(`pio test -e native -f test_web_export`) and `tools/regmap.py`, then writes `gen/manifest.json` with the hash of
every source. `npm run gen:check` names the sources that changed since.

| File | Content | Used by |
|------|---------|---------|
| `registry.json` | Settings: types, ranges, defaults, options, capabilities, bridged keys | app (trimmed), simulator |
| `menu.json` | The menu tree: ids (label paths), labels, Easy/Advanced flags, options, choices, greyed-row reasons | app, simulator |
| `actions-all.json`, `actions.<board>[.dev].json` | Actions with flags and parameters; `/api/actions` per board | app (trimmed), simulator |
| `schema.<board>[.dev].json`, `values.<board>.json` | `/api/schema` and default values per board | simulator |
| `routes.json` | `API_ROUTES_NORMAL` and `API_ROUTES_SAFE` | simulator, parity |
| `predicates.json` | Truth table of the menu visibility rules | unit tests |
| `validate.json` | The firmware's answers to the requests in `sim/vectors/` | simulator contract test |
| `catalog.json` | Console names for new profiles | app |
| `logsample.json` | Log line format | simulator contract test |
| `regmap.json` | Register map (ADV7280, ADV7391, TV5725) | simulator (`GET /api/regmap`) |

The page embeds trimmed copies of the registry and the actions (`tools/app-data.mjs`); the schema and the action list
are never fetched. The register map is not in the page: the Developer tools load it from `GET /api/regmap` when a
register section opens.

## Simulator

`sim/` is a device written in TypeScript (`npm run sim -- --board standard --port 8080 --ws 8081 --page
dist/app.html`; the defaults are a Pro board and these ports). It answers every route of `API_ROUTES_NORMAL` and
`API_ROUTES_SAFE` and the WebSocket like the firmware: the request validators are ports of the C++ ones, checked
against `gen/validate.json`; bridged settings send no WebSocket echo; the Host/Origin guard and the admission rules
apply.

Test endpoints (never in `gen/routes.json`):

| Endpoint | Body | Effect |
|----------|------|--------|
| `POST /__sim/scenario` | `{"name"}` | A fresh device in a named situation (`sim/scenarios.ts`): Pro and standard boards on each input, no signal, auto search, unsaved profile, update offers, ADV Controller absent or old, 36 profiles, safe mode, low memory, long texts |
| `POST /__sim/fault` | `{"name", "on"}` | A fault from `FAULTS` in `sim/simapi.ts`: busy `503`s, full queue, WebSocket drop, refusal or silence, slow or failed upload and restore, missing register map, ADV Controller not answering, ... |
| `POST /__sim/net` | `{"profile"}` | Network timing: `fast`, `device` (measured on a unit) or `slow` |
| `POST /__sim/tv` | `{"key", "value"}` | A change made on the TV or the OLED |
| `POST /__sim/live` | `{"name", "value"}` | A live readback (`/api/live`) |
| `POST /__sim/log`, `POST /__sim/event` | `{"cat", "level", "msg"}`, `{"type", "a", "b"}` | A console line, a history event |
| `GET /__sim/requests` | | What the page sent, the most requests in flight, requests made during an upload |
| `GET /__sim/test-page?case=` | | Small pages that exercise the Playwright checks |
| `GET /__sim/components` | | The row components page |

## Tests

| Command | What it runs |
|---------|--------------|
| `npm run test:unit` | Node's test runner on `test/unit/`: store, client, WebSocket, model, predicates against `gen/predicates.json`, layout, simulator contract, `gen/` freshness, parity, size gate |
| `npm run parity` | Every setting, menu node, action and route of `gen/` is used in `app/`, or listed with a reason in `test/parity-exclusions.json` (unused exclusions fail too) |
| `npm run test:e2e` | Playwright against the simulator, quick run: `phone-pixel8`, `phone-iphone15`, `desktop-chromium-1280` |
| `GBSC_E2E=full npm run test:e2e` | Every project (8: phones, tablet, Chromium, Firefox and WebKit desktops); `-- --shard=1/4` splits it |
| `GBSC_DEVICE=<ip> npx playwright test --project device` | The `@device` tests against a real unit (read-only) |

- The Playwright fixtures (`test/e2e/fixtures.ts`) give each worker a simulator and each test a fresh scenario. A
  test fails on console errors (allow one with `expectConsoleError(page, /re/)`), on any request outside the
  device, and when the page keeps more than 2 requests in flight. Layout, accessibility and speed checks are in
  `test/e2e/checks.ts`.
- The global setup builds `dist/app.html` and the components page before the run; `gen/` must exist.
- On a macOS host the WebKit projects (`phone-iphone15`, `tablet`, `desktop-webkit-1280`) are left out, because
  Playwright's WebKit leaks memory there: `docker compose run --rm dev check web --full` and CI run them.
- `GBSC_PW_WORKERS` sets the number of workers (default 3).

## npm scripts

| Script | What it does |
|--------|--------------|
| `gen` | Firmware tables into `gen/` |
| `gen:check` | Fails when `gen/` is older than the firmware sources |
| `build`, `build:app` | `dist/app.html` (one inlined page, no external URL) and its `.gz`, then the size gate |
| `size` | The gzip size of the page against the limit (`tools/size.mjs`) |
| `sim` | The simulator |
| `test:unit` | Unit tests |
| `parity` | Parity check |
| `test:e2e` | Playwright |
| `check:static` | `gen:check`, `tsc`, `test:unit`, `build:app`, `parity` |
| `check` | `check:static`, then `test:e2e` |

Other tools in `tools/`: `icons-app.mjs` and `icons-subset.py` (the Material Symbols subset the page embeds; the build
fails on an icon the font lacks), `fonts.mjs` and `logo-outline.py` (the wordmark as SVG paths in `app/art/logo.tsx`),
`app-shot.mjs` (screenshots of the built app against the simulator).

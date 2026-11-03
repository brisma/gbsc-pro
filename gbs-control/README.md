# GBS-Control

GBS-Control is the ESP8266 firmware of the GBSC PRO. It drives the TV5725 scaler, the TV menu (STV9426 OSD), the
OLED and its rotary knob, the IR remote, the volume chip, WiFi with the web app and its API, updates, and the link
to the ADV Controller (the HC32F460 board with the ADV7280 decoder and the ADV7391 encoder). The same image runs on a
standard gbs-control board (GBS-8200/8220 with an ESP8266): the board is detected at start and the features it lacks
are hidden.

It is built on [gbs-control](https://github.com/ramapcsx2/gbs-control): the upstream scaler code stays in
`gbs-control.ino`, and the GBSC PRO additions live in `pro/`.

- Installing, building, testing and flashing: [README](../README.md) and [CONTRIBUTING](../CONTRIBUTING.md).
- The web app the firmware serves: [public/README.md](public/README.md).
- The other side of the link: [adv-controller/](../adv-controller/README.md) and `shared/adv-link/`.

## Build and test

`python tools/dev.py build --esp` (from the repository root) builds the firmware with the web app into `out/`, and
`python tools/dev.py check esp` runs the native tests. Inside `gbs-control/`, PlatformIO works directly:

```bash
pio run                                   # firmware (env gbsc-pro, the default), web app included
pio run -t upload                         # flash over USB
pio test -e native                        # every native test
pio test -e native -f test_menu_engine    # one test
```

### PlatformIO environments

| Environment | What it builds |
|-------------|----------------|
| `gbsc-pro` | The firmware: ESP8266 (`d1_mini`) at 160 MHz, `eagle.flash.4m1m.ld` (1 MB LittleFS), ESPAsyncWebServer, ESPAsyncTCP, ThingPulse SSD1306 driver, IRremoteESP8266; `../shared` holds the `adv-link` library. |
| `native` | The host tests (Unity). `build_src_filter` lists the pure sources compiled on the host, plus the fakes in `test/sim/`. |

### Pre-scripts

| Script | Environment | What it does |
|--------|-------------|--------------|
| `tools/adv-controller-image.py` | `gbsc-pro` | Embeds `../adv-controller/release/adv-controller.bin` (in git) as `pro/link/adv-controller-image.gen.h`: checks the image's `ADVC` header at 0x400 and its CRC32, and regenerates the file only when the image changed. `--self-test` checks the script itself. |
| `tools/webui-build.py` | `gbsc-pro` | Builds `regmap_gz.h` when it is missing or stale (the register map served by `GET /api/regmap`) with `../tools/regmap.py` from `shared/adv-regmap/` and `tv5725.h`; `GBSC_NO_REGMAP=1` builds an empty one. Then builds the web app when `webui_html.h` is missing or older than its sources: `npm ci` when `node_modules` is stale, `npm run gen` when `npm run gen:check` fails, `npm run build:app`, and writes `webui_html.h` (the page gzipped). |
| `tools/native-paths.py` | `native` | Defines `GOLDEN_DIR` (`test/golden`) and `WEB_DIR` (`public`) for the tests. |

`webui_html.h`, `regmap_gz.h` and `pro/link/adv-controller-image.gen.h` are generated and not in git. The web app
build needs Node.js and npm, PlatformIO with a host C++ compiler (for `npm run gen`) and Python with PyYAML, fonttools
and brotli; `python tools/dev.py doctor` lists what is missing.

`tools/make-1200p-preset.py`, run by hand, builds `ntsc_1920x1200.h` and `pal_1920x1200.h` from the 1280x1024
presets.

## Code layout

### Upstream files

| File | What it holds |
|------|---------------|
| `gbs-control.ino` | The scaler: presets, sync watcher, mode detection, frame lock, the serial and legacy command handlers, WiFi and web server start; `setup()` and `loop()` call the `pro/` modules. |
| `options.h` | `userOptions` (`uopt`, the working settings) and `runTimeOptions` (`rto`); the GBSC PRO fields come from `pro/options-pro.h`. |
| `tv5725.h` | TV5725 register fields. |
| `ntsc_*.h`, `pal_*.h`, `ofw_RGBS.h`, `preset*Section.h` | Preset register tables. |
| `OSDManager.*` | The TV5725 icon OSD, used on the standard board. |
| `framesync.h`, `fastpin.h`, `tw.h` | Frame lock, fast pin access, I2C helpers. |
| `PersWiFiManager.*`, `src/` | WiFi manager, WebSockets and si5351mcu, compiled with the firmware; `3rdparty/` keeps the unmodified WebSockets and PersWiFiManager sources with their licences. |

### `pro/`

Most modules have a pure part (no Arduino calls, compiled in the `native` environment and tested on the host) and a
device part (`*-device.cpp`, `*-web.cpp` and similar) that only the firmware build compiles.

| Folder | What it does |
|--------|--------------|
| `gbs-control-pro.{h,cpp}` | Firmware version (`GBS_FW_VERSION`) and build channel, IR receiver, input path switching, picture colour, sharpness and scanline registers, 75 ohm sync termination. |
| `options-pro.h` | The GBSC PRO fields of `userOptions` (`USER_OPTIONS_PRO_FIELDS`). |
| `settings/` | The settings registry, its files, the debounced saver, request validation, the bridge to legacy command handlers, apply groups, `/api/settings` and `/api/schema`. |
| `profiles/` | Named profiles: store, file format, list, names and regions, register modes, auto-load, JSON and the `/api/profiles*` handlers. |
| `ui/` | The menu (tree, strings, engine, model, backend), the OSD and OLED renderers, themes, IR and encoder input, on-screen keyboard, console name catalog, the TV5725 icon OSD of the standard board, device glue (`menu-device.cpp`). |
| `drivers/` | STV9426 OSD chip (RAM map, shadow buffer, PLL timing, user-defined characters) and PT2257 volume. |
| `input/` | Input types, the automatic input state machine, the SCART cable identification and its device glue. |
| `link/` | ESP side of the ADV Controller link: state from the settings, send scheduler, requests (PING, INFO, SIGNAL), firmware updater, embedded image, 2X guard. |
| `api/` | The HTTP router and route table, the action table, request admission, Host/Origin guard, queues, backup and restore, live readbacks, Developer snapshots, the register map route, network diagnostics. |
| `update/` | Firmware upload, image checks, OTA over HTTP (channels, manifest, client), safe mode, the RTC record. |
| `net/` | WiFi optimisation (SDRAM clock choice) and the gateway link watch. |
| `log/` | Console log ring, line splitter for upstream prints, signal line, event history, loop watch. |
| `platform/` | Board detection and capability mask. |
| `picture/` | Colour and sharpness maths for the TV5725 registers. |
| `screen/` | Mask position (moving the display blanking window). |
| `adv/` | ADV presets and macros stored on the device (`/api/adv/*`). |
| `clock/` | Si5351 helpers for the Developer measurements. |
| `util/` | Flash strings in pure code, bounded string copy. |

## How it works

### Main loop and the network context

ESPAsyncWebServer handlers run in the network context: they only validate a request and queue it. `loop()` applies
the queues (`apiLoop()`), then runs profile operations and auto-load, the ADV Controller link, the menu and its
renderers, the OSD driver, OTA, and the upstream scaler work. While a firmware upload runs, the profile work and
the scaler work are skipped.

### Settings

Every persisted setting is one line of `SETTINGS_TABLE` in `pro/settings/settings-table.h`:

```
SETTING(id, key, type, uopt field, default, min, max, scope, apply groups, options, flags, caps)
```

- `SID_<id>` is the setting's index, used by the menu tree.
- Type `Bool`, `U8`, `U16` or `Enum`; enum options are a string in `SETTINGS_OPTIONS` (`"Name=value|..."`; `@<cap>`
  needs a capability, `#a` shows only in the Advanced menu).
- Scope `Device` is saved in `/cfg/device.cfg`; scope `Profile` in `/cfg/current.cfg` and in profile files. Files
  are `key=value` lines, written to `<path>.tmp` and renamed, 2 s after the last change (`settings-saver.h`).
- Apply groups (`APPLY_*` in `settings-types.h`) name the side effect of a change; `settingsApply()`
  (`settings-apply.cpp`) runs them, and `APPLY_PRESET` reloads the preset from the loop.
- `caps` hides a setting on boards without that hardware (`CAP_ADV`, `CAP_OSD`, ...).
- String settings have their own table, `SETTINGS_STR_TABLE`, with the flags `SETTING_FLAG_SECRET` (write-only over
  HTTP, never in backups) and `SETTING_FLAG_DEVICE_ONLY` (read-only over HTTP).
- Some settings reuse an upstream command handler for their register writes (`SETTINGS_BRIDGE_TABLE` in
  `settings-bridge.cpp`).

A change from the web, the OSD or the OLED goes through the same queue and is broadcast on the WebSocket (port 81)
as `s:<key>=<value>`.

### Profiles

Up to 36 profiles in `/profiles/pNN.cfg` (16 KB at most): `profile.*` header lines (name, region, signal
fingerprint), the Profile-scope settings, and `reg.<mode>.<n>` lines with the scaler registers captured per mode.
Load, save, rename, delete, import and the others go through one queue (`profileQueueOp()`), run by
`profileOpsLoop()`, for the web, the OSD and the OLED alike. With `profiles.autoload` on, a profile whose fingerprint
matches a stable signal loads by itself (`profile-autoload.*`).

### Menu

One menu tree for the TV OSD and the OLED: `pro/ui/menu-tree.cpp` (PROGMEM nodes; settings by `SID_`, actions by
`AID_`, engine functions by `FN_`), strings in `pro/ui/menu-strings.h` (STV9426 ROM characters only). Nodes flagged
`NF_ADVANCED` are hidden in the Easy menu mode (`system.menu_mode`). The engine (`menu-engine.*`) handles
navigation, edits, confirmations, the revert countdown and overlays; it reads and writes only through `MenuBackend`
(`menu-backend-device.cpp` on the device, `test/sim/menu-fake.*` on the host). `osd-renderer.*` draws on the STV9426,
`oled-renderer.*` on the SSD1306. On the standard board the Quick menu also opens the upstream TV5725 icon OSD
(`tv-osd.*`).

### Web API

Every route is one row of `API_ROUTES_NORMAL` (or `API_ROUTES_SAFE` in safe mode) in `pro/api/api-route-match.h`,
served by one router (`api-router.cpp`); handlers are declared in `api-router.h`. Paths are exact, except the
`/api/actions/` prefix.

| Route | Purpose |
|-------|---------|
| `/` | The web app (`webui_html.h`, gzip, ETag = build id) |
| `/api/info`, `/api/live`, `/api/log`, `/api/events` | Status, menu readbacks, console log, event history |
| `/api/settings`, `/api/schema` | Values (GET, POST) and the registry |
| `/api/actions`, `/api/actions/<name>` | The action list and running one |
| `/api/profiles*` | Profile list, operations, export and import |
| `/api/system/backup`, `/restore`, `/update` | Backup file, restore, firmware upload |
| `/api/wifi*` | WiFi status, scan, connect, reset |
| `/api/dev/*`, `/api/adv/*`, `/api/regmap` | Developer register tools, ADV presets and macros, the register map |

- POST under `/api/` needs the header `X-GBSC: 1`; Host and Origin must be an IPv4 address, the device's host name
  or an entry of `system.extra_hosts` (`api-guard.*`).
- Admission (`api-admission.*`) answers `503` with `X-GBSC-Busy` and `Retry-After` when the heap is low or too
  many connections are open, before a handler allocates anything; a firmware upload runs alone.
- Actions are the rows of `ACTIONS_TABLE` in `pro/api/api-actions-table.h`: `ACTION(id, name, caps, flags, kind,
  arg)`. Kind `ACT_SERIAL` or `ACT_USER` queues an upstream command character; `ACT_FUNC` runs an `AF_*` case in
  `api-device.cpp`. Flags mark Developer-only actions, confirmations, parameters, and register changes that make
  the active profile unsaved (`ACTION_REGS`).

### ADV Controller link

The protocol (frames, commands, the ADV state, the diff planner) is `shared/adv-link/adv-link.{h,c}`, compiled into
both firmwares. The ESP is the source of truth: `adv-state.cpp` builds the ADV state from the settings,
`adv-link-sched.cpp` decides when to send it, and `adv-link-device.cpp` sends a STATE frame on UART0 TX (GPIO1)
and reads replies on the same pin. The ADV Controller answers INFO (version, build id, capabilities, the hash of
the state it applied, which the ESP checks) and reports the decoder signal (`adv-link-client.*`). `adv-updater.*` uploads the embedded image when it differs from the
running one.

### Updates

- Upload: `POST /api/system/update` takes a gzip or raw image (`update-device.cpp`); the loop checks it before the
  restart.
- OTA: Stable and Beta channels over plain HTTP from `GBSC_OTA_BASE` (`<channel>/manifest.txt` and the image;
  `ota-plan.*`, `update-manifest.*`, `ota-client.cpp`, `ota-update.cpp`). The build channel and version suffix
  come from `GBS_FW_CHANNEL` and `GBS_FW_VERSION_SUFFIX` (set by `tools/release.py`); a local build is
  `X.Y.Z-dev0` on Beta.
- Safe mode after 3 crash resets in a row: only `API_ROUTES_SAFE` (info, log, backup, upload, restart) and a small
  built-in page.
- Arduino IDE upload over WiFi needs Developer mode and `system.ota_password`.

### Board detection

`platformDetect()` probes the I2C bus for the STV9426 (0x5D), the PT2257 (0x44) and the OLED (0x3C). A Pro chip
makes the board a GBSC PRO; `system.board` (Auto, Standard, Pro) can override the features, while the use of
GPIO1, GPIO2 and GPIO15 always follows the probes (`platformLinkHardware()`). `hwHas(CAP_...)` tells what is
present; settings, enum options and actions with missing capabilities are hidden from the menu and the API.

### Console log

`logPrintf(LC_<category>, LL_INFO | LL_DEBUG, PSTR(...))` writes a line with category and uptime into a 2 KB RAM
ring (`GET /api/log?since=`), to the WebSocket (`l:`) and, where GPIO1 is free, to the USB serial. Debug lines need
`system.devmode`. Upstream `SerialM` prints become `[gbs]` lines (`logTagNext()` sets their category). The event
history (`GET /api/events`, WebSocket `e:`) keeps the last 20 input, profile, resolution and update events.

## Tests

The native tests are in `test/test_*/test_main.cpp`, one folder per module. Host fakes and helpers are in
`test/sim/`:

| File | What it is |
|------|------------|
| `menu-fake.*` | `MenuBackend` on the real settings registry |
| `stv-fake.*` | STV9426 RAM and registers, with a decoder that turns the RAM back into the displayed screen |
| `oled-host.h`, `oled-lib.cpp`, `shim/Arduino.h` | The real ThingPulse `OLEDDisplay` compiled on the host |
| `menu-rig.h`, `menu-find.h`, `menu-fit.h` | Engine rig, node lookup by label path, fit walk for the renderers |
| `golden.*`, `host-fs.h` | Golden file checks, file helpers for macOS, Linux and Windows |

- Golden screens: `test/golden/osd/*.txt` and `test/golden/oled/*.pbm`, checked by `test_osd_render`,
  `test_oled_render` and `test_stv9426_driver`. After a deliberate change, `GOLDEN_UPDATE=1 pio test -e native`
  rewrites them; a difference writes `<name>.actual.<ext>` beside the golden.
- `test_screens` renders every menu page and overlay on both displays; with `SCREENS_OUT=<dir>` it keeps them, and
  `../tools/osd-golden-report.py` and `../tools/oled-golden-report.py` turn them (or the goldens) into HTML.
- `test_web_export` exports the registry, actions, routes, menu tree and validators as JSON for the web app
  (`npm run gen` in `public/`).

## Adding things

### A setting

1. A field in `USER_OPTIONS_PRO_FIELDS` (`pro/options-pro.h`), unless an upstream field in `options.h` fits.
2. One `SETTING(...)` line in `pro/settings/settings-table.h` (and an `OPT(...)` line for new enum options). Its
   apply groups pick the side effect; a new one goes into `settingsApply()` in `settings-apply.cpp`.
3. A menu node if it belongs in the menu (below).
4. In `public/`: `npm run gen`. The web app reads the registry from `gen/`; the setting must be used by the app (a
   row in `app/layout/pages.ts`) or listed in `test/parity-exclusions.json` with a reason.

The API, the settings files, backups and profiles follow the table.

### A menu item

1. The label (`S_...`) and its help line (`H_...`) in `MENU_STRINGS` in `pro/ui/menu-strings.h`.
2. One node line in `pro/ui/menu-tree.cpp` with the macros at the top of the tree (`SET`, `ACT`, `STEP`, `CHO`,
   `SUB`, ...): label, help, the setting or action, a visibility predicate (`P_*`) and flags (`NF_ADVANCED`,
   `NF_CONFIRM`, `NF_REVERT`, ...).
3. The native tests check the characters, the bindings, duplicate labels and the fit on the TV and the OLED;
   `test_easy_pages_*` in `test_menu_tree` pin the Easy pages, and golden screens may need `GOLDEN_UPDATE=1`.
4. In `public/`: `npm run gen`, then show the node in the web app (`app/layout/pages.ts`) or list it in
   `test/parity-exclusions.json`.

### An API action

1. One `ACTION(...)` line in `pro/api/api-actions-table.h`.
2. For `ACT_FUNC`: an `AF_*` id in `api-actions.h` and its case in `api-device.cpp`; byte parameters
   (`ACTION_PARAM_BYTES`) also need their rule in `kParamRules` (`api-actions.cpp`).
3. In `public/`: `npm run gen`. The simulator validates it from `gen/`; its effect, if a test needs one, goes in
   `sim/`; the app uses it or lists it in `test/parity-exclusions.json`.

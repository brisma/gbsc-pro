# Contributing to GBSC PRO

Bug reports and ideas are welcome in the [issues](https://github.com/brisma/gbsc-pro/issues) and on the
[Discord](https://discord.com/invite/2MMWRkVRbk). Pull requests go to `main`; every pull request runs the tests on
Windows, macOS and Linux.

## Repository layout

| Folder | What it holds |
|--------|---------------|
| [gbs-control/](gbs-control/) | GBS-Control, the ESP8266 firmware (PlatformIO, Arduino): the TV5725 scaler, the TV menu (STV9426 OSD), the OLED and knob, the IR remote, WiFi and the web API. Its native unit tests are in `test/`. |
| [gbs-control/public/](gbs-control/public/) | The web app (TypeScript, Preact) served by the firmware, its device simulator and its tests (unit and Playwright). |
| [adv-controller/](adv-controller/) | The ADV Controller firmware (HC32F460, GNU make): the ADV7280 decoder and the ADV7391 encoder; `release/adv-controller.bin` is the image GBS-Control embeds and updates. |
| [shared/](shared/) | The link protocol between the two firmwares (`adv-link`) and the ADV register maps (`adv-regmap`). |
| [gbsc-pro-flasher/](gbsc-pro-flasher/) | The GBSC PRO Flasher (Rust, Tauri 2): app and command line. |
| [tools/](tools/) | `dev.py` (build and test), the release, OTA and flasher packaging scripts. |
| [hardware/](hardware/) | Gerber files, BOM and datasheets. |

## Building and testing

One command, `tools/dev.py` (`python3` on macOS and Linux, `python` on Windows), builds both firmwares and runs every
test the same way on Windows, macOS and Linux, either in the development container or with the tools installed on
your computer.

| Command | What it does |
|---------|--------------|
| `python tools/dev.py doctor` | lists the tools each part needs: found, missing (with how to install it) or another version |
| `python tools/dev.py check` | every test suite (`gen`, `esp`, `adv`, `web`, `flasher`, `tools`), or only the ones named: `check esp adv` |
| `python tools/dev.py check --full` | the web app on every browser and device (8 projects) instead of the quick three |
| `python tools/dev.py build` | GBS-Control (with the web app) and the ADV Controller into `out/`; `--esp` or `--adv` for one (a standard gbs-control board needs only `--esp`) |
| `python tools/dev.py flash` | flashes `out/` with the GBSC PRO Flasher command line: USB (`--port P`) or WiFi (`--host gbscontrol.local`; `--allow-downgrade` to put a local `X.Y.Z-dev0` build over the release `X.Y.Z`); `--esp` / `--adv` for one |

A local build reports the version `X.Y.Z-dev0`: it sorts below every release, so the unit is still offered the real
ones, and it follows the Beta channel.

### In the development container (only Docker and git needed)

```bash
docker compose run --rm dev doctor
docker compose run --rm dev check
docker compose run --rm dev build
docker compose run --rm dev bash      # a shell inside
```

The image (`docker/Dockerfile`, Ubuntu 24.04 with every toolchain pinned) is pulled from `ghcr.io/brisma/gbsc-pro-dev`
or built from the Dockerfile (`docker compose build dev`). VS Code users can also open the folder and choose "Reopen
in Container" (`.devcontainer/`: it runs that image, so pull or build it first). Any Docker engine works: Docker
Desktop, Podman Desktop, Colima, Rancher Desktop.
- On Windows, clone the repository inside WSL2 (`/home/...`, not `C:\`): builds on the Windows drive are slow.
- On a Linux host, run `mkdir -p gbs-control/public/node_modules gbsc-pro-flasher/ui/node_modules` once first, or
  Docker creates those folders as root.
- Two clones in folders with the same name share the container's caches: give one of them its own
  `COMPOSE_PROJECT_NAME`.
- Flashing over USB is done on the computer, not in the container (`dev.py flash`).

### On your computer

| All systems | Windows | macOS | Linux |
|-------------|---------|-------|-------|
| git, Python 3.11+ with PlatformIO and the build scripts' packages (below), Node.js 26, Rust (https://rustup.rs), Arm GNU Toolchain 15.3.rel2 | MSYS2 (https://www.msys2.org) with `pacman -S make mingw-w64-ucrt-x86_64-gcc` and `C:\msys64\ucrt64\bin` on `PATH`; Visual Studio Build Tools (Desktop development with C++) | Xcode Command Line Tools (`xcode-select --install`); the Arm toolchain with `brew install --cask gcc-arm-embedded` | `build-essential pkg-config libwebkit2gtk-4.1-dev libxdo-dev libssl-dev libayatana-appindicator3-dev librsvg2-dev libudev-dev` |

The Python tools go in a virtual environment (macOS and most Linux systems refuse a system-wide `pip install`):

```bash
python3 -m venv .venv && . .venv/bin/activate        # Windows: python -m venv .venv  then  .venv\Scripts\activate
pip install platformio==6.2.0 pyyaml fonttools brotli
```

With the environment active, `python tools/dev.py doctor` tells what is still missing, and
`npx --prefix gbs-control/public playwright install --with-deps` installs the test browsers.
- On macOS the WebKit browsers (iPhone, iPad, Safari) are left out of the web tests: Playwright's WebKit leaks memory
  there; the container and CI run them.
- On Windows, builds are much faster with the clone and the tool folders (`%USERPROFILE%\.platformio`,
  `%USERPROFILE%\.cargo`, `C:\msys64`) excluded from Microsoft Defender's real-time scanning (Windows Security >
  Virus & threat protection > Exclusions).
- Only after the firmware? `cd gbs-control && pio run` (VS Code with PlatformIO works too) builds GBS-Control with the
  web app, and `cd adv-controller && make` the ADV Controller.
- The GBSC PRO Flasher packages are built on each system: `python tools/flasher-build.py --mac` (macOS),
  `--windows-native` (Windows), `--linux-native` (Linux x86-64), `--out <dir>`.

### Continuous integration

- Every push to `main` and every pull request (`.github/workflows/test.yml`): every suite in the container with the
  quick web run, and every suite but the browsers natively on Windows, macOS and Linux.
- Before a release, the full run (every browser and device, split into four parallel jobs: `test-full.yml`).
- Line endings are LF in every checkout (`.gitattributes`); a Windows clone made before that file existed needs
  `git add --renormalize .` or a fresh clone.

## Conventions

- Code, comments and documentation are in English.
- Comments explain the code: what it does and why, hardware facts, datasheet or upstream references, issue numbers.
  The story of a change belongs in its pull request and in the changelog, not in the code.
- Tests check behaviour and functions (the native tests for the firmware, `make test` for the ADV Controller, unit
  and Playwright for the web app, `cargo test` for the flasher). Screens are checked against golden files
  (`gbs-control/test/golden/`), regenerated with `GOLDEN_UPDATE=1` after a deliberate change.
- A new persisted setting is one line in `gbs-control/pro/settings/settings-table.h` (plus its field in
  `options-pro.h`); the web app reads it from the API, nothing else to sync.
- A new menu item is a string in `pro/ui/menu-strings.h` and a node in `pro/ui/menu-tree.cpp`; the tests check that
  it fits the TV and the OLED.
- Every change for users adds a line to the `## Unreleased` section of [CHANGELOG.md](CHANGELOG.md), in the same
  commit. A release empties that section and removes its heading: put it back on top, above the newest version.

## Releases

Releases are tags on `main`, and the GitHub Actions workflows build and publish them:
- `vX.Y.Z-betaN`: a Beta, a release candidate already tested, waiting for users' feedback (GitHub pre-release and
  the Beta update channel);
- `vX.Y.Z`: the promotion of the last Beta to Stable (GitHub release and the Stable channel), on the commit that only
  dates the version's CHANGELOG section: the code is the Beta's.

Each release carries the GBS-Control image (`.bin` for USB, `.bin.gz` for WiFi and updates), the ADV Controller image,
the GBSC PRO Flasher for macOS, Windows and Linux, and one zip with all of it. The changelog section of the version is
the release notes.

Units update over WiFi from a plain web server; what it must hold and how it must answer, for a fork that publishes
its own builds: [tools/ota/README.md](tools/ota/README.md).

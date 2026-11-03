# GBSC PRO Flasher

A desktop app and a command line to flash and update GBSC PRO units:
- over **USB**: GBS-Control (ESP8266) and the ADV Controller (through its bootloader), with an optional erase of the
  ESP8266;
- over **WiFi**: GBS-Control updates, the ADV Controller update from the image inside GBS-Control, backups and
  profiles.

The app and the command line share one Rust core. The app is a Tauri 2 shell: one native program per system, no
runtime to install.

## For users

### Where to get it

Every [release](https://github.com/brisma/gbsc-pro/releases) carries the flasher inside `gbsc-pro-<version>.zip`,
next to that release's firmware files, and also on its own (`GBSC-PRO-Flasher-macOS.zip`,
`GBSC-PRO-Flasher-Windows.zip`, `GBSC-PRO-Flasher-Linux.zip`). Unzip the release and keep the flasher in that folder:
it finds the firmware files next to it and preselects them. The first install step by step is in the
[main README](../README.md#first-install-usb).

| System | App | Command line |
|--------|-----|--------------|
| macOS (Apple Silicon and Intel) | `GBSC PRO Flasher.app` | `gbsc-flasher-macos` |
| Windows (x86-64) | `GBSC PRO Flasher.exe`, a portable program (it uses the Microsoft Edge WebView2 runtime) | `gbsc-flasher.exe` |
| Linux (x86-64) | `gbsc-pro-flasher.AppImage`, or the `gbsc-pro-flasher.deb` package | `gbsc-flasher-linux` |

`esp8266-stub.json` and `LICENSES/` come with every package: keep them next to the program (see
[The ESP8266 stub](#the-esp8266-stub)).

### First start

- **macOS**: the app is not signed by Apple. Right-click it, choose Open, then Open again (once). If the app warns that
  macOS runs it from a temporary place, it cannot see the firmware files next to it: move the folder out of Downloads,
  or run `xattr -dr com.apple.quarantine <folder>`, and open it again. The command line is blocked the same way:
  `xattr -d com.apple.quarantine gbsc-flasher-macos`. The first time the app looks for or connects to a GBSC, macOS
  asks for the Local Network permission: allow it (System Settings > Privacy & Security > Local Network).
- **Windows**: SmartScreen may warn about an unknown publisher: choose "More info", then "Run anyway". Extract the zip
  first: started from inside the zip, the program does not see the firmware files (it says so). The first "Find on the
  network" asks for the Windows Firewall permission: choose Allow, or the GBSC's answers are blocked.
- **Linux**: `chmod +x gbsc-pro-flasher.AppImage`. Your account needs access to serial ports (usually the `dialout`
  group, `uucp` on some distributions). The `.deb` installs the app system-wide, so it has no release folder to
  preselect from: choose the firmware files by hand.

### The app

The app has three areas, chosen at the top. USB and WiFi are numbered steps: a finished step folds into one line with
Change, the next one opens. One job runs at a time, and its progress and messages show at the bottom.

**USB**
1. **Device**: the GBSC devices plugged in, updated live as cables come and go.
   - *GBS-Control (ESP8266)*: the GBSC's own USB port.
   - *ADV Controller (bootloader)*: on a GBSC PRO, **hold the ADV Controller's button while you plug its USB-C cable**.
     Plugged without the button, the ADV Controller runs its firmware and shows up as *ADV Controller (application)*:
     that one cannot be flashed, and the app says what to do.
   - A USB device without the bootloader's USB IDs is listed as "maybe the ADV Controller": the app asks before
     flashing it, since it may be another board.
2. **Firmware**: the file found next to the app (the `.bin` for USB), a file you drop on the window, or Choose a file.
   The app reads its kind and version and refuses a file made for the other chip. A file with neither a GBSC header nor
   a version marker (a factory or upstream image) needs a confirmation. For the ESP8266, "No file: erase only" skips
   the write.
3. **Write** (or **Erase**):
   - ESP8266: before writing, Keep everything, Erase settings and profiles, Erase WiFi settings, or Erase everything;
     every erase asks first. With the stub the write takes about 25 s and is verified on the chip; without it (slow
     mode) about 90 s, not verified. The GBSC restarts by itself.
   - ADV Controller: about 10 s. Its bootloader returns no checksum, so the last step checks it over WiFi.
4. **Done**: what happened and what to do with the cable. After an ADV Controller flash, unplug the USB-C cable, power
   the GBSC normally, type its address and press Check: the app compares the build the ADV Controller reports with the
   flashed file.

Erasing settings and profiles, or everything, without writing a file also erases the GBS-Control firmware: the GBSC
does not start until you flash one.

**WiFi**
1. **GBSC**: Find on the network (every unit answers to `gbscontrol.local`) or type its IP address (TV menu: System >
   WiFi), then Connect. The step shows the GBS-Control and ADV Controller versions, and safe mode when the unit is in
   it.
2. **GBS-Control image**: the `.bin.gz` found next to the app, a dropped file or Choose a file (`.bin` or `.bin.gz`).
   An ADV Controller image is refused here: over WiFi the ADV Controller updates from GBS-Control.
3. **Install**: the app asks before installing an older version or a file without the GBSC marker, uploads the image,
   waits for the restart and reads the version the GBSC came back with. Settings and profiles stay. When the unit
   offers an ADV Controller update (a newer image is built into its GBS-Control), **Update the ADV Controller** starts
   it; the picture stops for about 20 s.

A unit in safe mode (its firmware crashed three times in a row) takes only the install and the backup download:
installing a GBS-Control image takes it out of safe mode.

**Backup**
- **Download backup**: the unit's settings and profiles in one text file (`gbsc-backup-<address>-<date>.txt`, in
  Documents by default).
- **Restore a backup**: the file is checked with the same rules as the web app first; the GBSC replaces its settings
  and profiles with the file's, then restarts.
- **Profiles**: Load lists them; Export saves one as `<name>.gbscprofile.txt`; Import a profile file adds one.

### The command line

`gbsc-flasher` (`gbsc-flasher-macos`, `gbsc-flasher.exe`, `gbsc-flasher-linux` in the release) does the same jobs
from a terminal. `gbsc-flasher --help` and `gbsc-flasher <command> --help` list everything.

| Command | What it does |
|---------|--------------|
| `list` | the GBSC devices on USB, one per line: port, then what it is |
| `flash [FILE] [--port P] [--erase user\|wifi\|all] [--no-stub] [--force]` | flashes a `.bin` or `.bin.gz` over USB; an ADV Controller image goes to the ADV Controller bootloader, any other to the ESP8266 |
| `update FILE --host H [--allow-downgrade] [--force]` | installs a GBS-Control image (`.bin.gz` or `.bin`) over WiFi and prints the version after the restart |
| `update-adv --host H` | starts the ADV Controller update from the image inside GBS-Control and waits for its end |
| `backup --host H [--out FILE]` | saves the settings and profiles, by default as `gbsc-backup-<host>-<YYYY-MM-DD>.txt` in the current folder |

Options:
- `--port`: the port to use. Without it, `flash` takes the only device that fits the file and stops when there are
  none or several. A generic USB modem without the ADV Controller bootloader's USB IDs is flashed only when named here.
- `--erase` (ESP8266 only, before the write): `user` erases the firmware, settings and profiles area, `wifi` the WiFi
  settings and radio calibration, `all` the whole flash. `flash --erase …` without a file only erases; with `user` or
  `all` the GBSC then needs a firmware before it starts.
- `--no-stub`: writes through the ESP8266 ROM loader instead of the stub (slow, not verified).
- `--force`: accepts a file with neither a GBSC header nor a version marker. `flash` then guesses the chip from the
  size; `update` would replace the GBSC PRO firmware with it.
- `--host`: an IP address or `gbscontrol.local`, optionally with `:port` for the HTTP port.
- `--allow-downgrade`: installs a file older than the version the unit runs.

The exit code is 0 on success, 1 on an error (the reason is printed), 2 on a usage error.

```sh
gbsc-flasher list
gbsc-flasher flash gbsc-pro-3.0.0.bin                                   # the only ESP8266 on USB
gbsc-flasher flash adv-controller-3.0.0.bin --port /dev/cu.usbmodem1101 # ADV Controller in its bootloader
gbsc-flasher flash gbsc-pro-3.0.0.bin --port COM5 --erase user          # erase settings and profiles, then write
gbsc-flasher flash --erase wifi                                         # erase the WiFi settings only
gbsc-flasher update gbsc-pro-3.0.0.bin.gz --host 192.168.1.50
gbsc-flasher update-adv --host gbscontrol.local
gbsc-flasher backup --host 192.168.1.50 --out my-gbsc.txt
```

## For developers

Building, testing, the development container and releases are in [CONTRIBUTING.md](../CONTRIBUTING.md); this section
covers the flasher's own parts.

### Layout

| Folder | What it holds |
|--------|---------------|
| `core/` | `gbsc-core`, the library every job runs in: firmware files, port detection, the serial link, the ESP8266 loader and stub, YMODEM for the ADV Controller, the device API over WiFi, the release folder, the version order |
| `cli/` | `gbsc-flasher`, the command line (clap) |
| `app/` | `gbsc-pro-flasher`, the Tauri 2 shell: commands over the core (`commands.rs`), the live USB device list (`hotplug.rs`), the bundle config (`tauri.conf.json`), the macOS Local Network entries (`Info.plist`) |
| `ui/` | the app's front end (Preact, TypeScript, built with esbuild into `ui/dist/index.html`). It reuses the web app's style tokens, icon font, logo and its backup and version modules from `gbs-control/public/app/` |
| `resources/` | `esp8266-stub.json`, `LICENSES/`, `STUB-SOURCE.md` |

### The core

| Module | What it does |
|--------|--------------|
| `image.rs` | what a file is. An ADV Controller image carries the `ADVC` header at offset 0x400 (version, length, CRC-32), and the length and CRC must match; a GBS-Control image carries the marker `GBSCVER=<version>;`. A gzip file is unpacked (at most 8 MB). A file with neither is classified by size and flagged `by_size_only` |
| `detect.rs` | port classification and choice (table below), one entry per physical device (macOS lists a port as `cu.*` and `tty.*`, and a CH340 under two drivers) |
| `link.rs` | the `SerialLink` trait the protocols talk through: a real port (`serialport`) or a simulated device in the tests |
| `esp/` | the ESP8266 loader: SLIP framing, command packets, the flash sequence, the stub file |
| `ymodem.rs` | the ADV Controller bootloader (HCMGBoot) |
| `device/` | a unit over WiFi: HTTP, the status WebSocket, mDNS |
| `release.rs` | the folder the user sees the app in, and the newest firmware files there |
| `version.rs` | the version order: `X.Y.Z-devN` < `X.Y.Z-betaN` < `X.Y.Z`, the same as the firmware, the web app and `tools/ota/version.py` |
| `error.rs`, `progress.rs` | flash errors with the text the user reads; progress events (status line, bytes, done) |

Port classification:

| Kind | Recognised by | Flashed |
|------|---------------|---------|
| ESP8266 | CH340 (1A86:7523), any Silicon Labs CP210x (VID 10C4), or those names in the description; `wchusbserial` ports | yes |
| ADV Controller bootloader | 2E88:4603, manufacturer XHSC, or "HCMGBoot"/"HC32" in the description | yes |
| ADV Controller application console | 1209:0001 "ADV Controller console" (on Windows, 1209:0001 with the generic serial device name) | no: refused with the way into the bootloader |
| Unconfirmed | any other `usbmodem` port | only when the user names it (`--port`) or confirms it in the app |

### ESP8266: ROM loader and stub

The sequence follows esptool 4.8.1:
1. Classic reset into the ROM loader with DTR and RTS, then SYNC at 115200 baud (up to seven resets).
2. With the stub: its text and data go into RAM (MEM_BEGIN, MEM_DATA, MEM_END), and the stub greets with `OHAI`. The
   link moves to 460800 baud (CHANGE_BAUD) and SPI_SET_PARAMS sets a 4 MB flash. Writes are deflate-compressed in
   16 KB blocks (FLASH_DEFL_*), and SPI_FLASH_MD5 compares the flash with the file. If the stub does not greet, the
   chip is reset and the ROM path takes over.
3. Without the stub: ROM FLASH_BEGIN with esptool's erase-size workaround for the ROM, then 1 KB FLASH_DATA blocks.
   The ROM can neither unpack, hash nor read the flash, so this path is not verified beyond each block's checksum.
4. A hard reset with RTS starts the new firmware.

The image header is set to DIO, 4 MB, 40 MHz, as esptool's `--flash_mode dio`. Erase areas: `user` 0x000000 to
0x3FC000 (firmware, file system, EEPROM), `wifi` 0x3FC000 to 0x400000 (radio calibration and the SDK's WiFi settings),
`all` the whole 4 MB. An image larger than 0x3FC000 is refused before the port is touched, so a write never reaches
the WiFi area.

### ADV Controller: HCMGBoot and YMODEM

At 115200 baud, with DTR and RTS raised first (Windows opens a port with DTR low):
1. `0x55` asks for 48 bytes of chip information (optional: the flash goes on without it).
2. `1` enters download mode; the bootloader then asks with `C`.
3. YMODEM: a header packet with the file name and size, then 128-byte packets padded with 0x1A, each with a
   CRC-16/XMODEM. A NAK resends the packet, up to 10 times; no answer stops the transfer instead of resending, since a
   packet whose ACK was lost may already be written. Then EOT and the closing empty packet.

HCMGBoot returns no checksum of what it wrote. The check is done over WiFi once the ADV Controller runs: the build id
it reports (`/api/info`, `adv_controller.build`) must equal the CRC-32 in the flashed image's `ADVC` header.

### The device API over WiFi

`device/` talks to a unit on firmware 3.x:
- HTTP on port 80. Every POST carries `X-GBSC: 1`; a 503 busy answer is retried after its `Retry-After`, up to five
  times. Endpoints used: `/api/info`, `/api/log` (the boot id, to see a restart), `/api/system/update` (multipart; a
  `.bin.gz` is sent as it is), `/api/actions/system.update_adv_controller`, `/api/system/backup`,
  `/api/system/restore`, `/api/profiles`, `/api/profiles/export`, `/api/profiles/import`.
- The status WebSocket on port 81 (subprotocol `arduino`, lines `s:status.<key>=<value>`) gives the update
  percentage and the results of restore and profile import. In safe mode there is no WebSocket: the flasher polls
  `/api/info` instead.
- After an update the flasher waits for the restart and reads the version again: a unit that comes back with another
  version than the file's is reported as not applied.
- Finding units: the system resolver for `gbscontrol.local`, plus an mDNS browse of `_http._tcp` for the instance
  `gbscontrol`.

### Files next to the app

`release.rs` finds the folder the user sees: the executable's folder, the folder that holds the `.app` on macOS, the
folder of the `.AppImage` (`$APPIMAGE`) on Linux. There it picks the newest GBS-Control and ADV Controller files
(`*.bin`, `*.bin.gz` with a marker or header): for the same version the `.bin` for USB and the `.bin.gz` for WiFi. It
also tells when macOS runs the app from a temporary path (App Translocation) or Windows runs it from inside a zip.

### Tests

From the repository root, `python tools/dev.py check flasher` (see [CONTRIBUTING.md](../CONTRIBUTING.md#building-and-testing))
installs the web app's and the UI's packages, builds the UI, runs the UI tests (`npm test` in `ui/`: Node tests of
`state.ts` and `store.ts`, then `tsc`) and `cargo test --workspace`.

The core is tested against simulated devices behind `SerialLink` (`core/tests/fakes/`):

| Fake | What it simulates |
|------|-------------------|
| `esp_rom.rs` | the ESP8266 loader: ROM commands with the ROM's own erase behaviour, the stub after MEM_END, deflate writes, MD5; faults: ignored SYNCs, a lost greeting, a port that dies, a corrupted byte, a slow flash |
| `hcmgboot.rs` | HCMGBoot: chip information, download mode, the YMODEM receiver; faults: no `C`, NAK, cancel, a lost ACK, no EOT ACK, a port that dies |
| `sim.rs` | a unit over WiFi: the web app's device simulator (`gbs-control/public/sim/main.ts`) started with Node on free ports; it needs `npm ci` and `npm run gen` in `gbs-control/public` |

The command line has its own tests (`cli/tests/cli.rs`: usage, refusals before any write, a backup against the
simulator), and the app has unit tests in `app/src/commands.rs`.

### Running the app in development

With the tools of [CONTRIBUTING.md](../CONTRIBUTING.md#on-your-computer), from the repository root:

```sh
npm ci --prefix gbs-control/public           # the UI build uses the web app's icon tools and styles
npm ci --prefix gbsc-pro-flasher/ui
npm run build --prefix gbsc-pro-flasher/ui   # ui/dist/index.html (needs Python with fontTools for the icon font)
cargo install tauri-cli --version '^2' --locked
cd gbsc-pro-flasher/app && cargo tauri dev
```

The app loads `ui/dist/` as built: after a change in `ui/src/`, run `npm run build` again and restart it. The command
line runs with `cargo run -p gbsc-flasher-cli -- <command>` from `gbsc-pro-flasher/`. It looks for
`esp8266-stub.json` next to its executable: copy the file from `resources/` into `target/debug/` or
`target/release/`, or it writes in slow mode.

### Building packages

`tools/flasher-build.py` builds the packages of the system it runs on into one folder, with `esp8266-stub.json` and
`LICENSES/`:

| Option | Runs on | Output |
|--------|---------|--------|
| `--mac` | macOS | `GBSC PRO Flasher.app` (universal: Apple Silicon and Intel, ad hoc signed), `gbsc-flasher-macos` |
| `--windows-native` | Windows | `GBSC PRO Flasher.exe`, `gbsc-flasher.exe` |
| `--windows` | macOS, cross-build with cargo-xwin | the same two Windows files |
| `--linux-native` | Linux x86-64 | `gbsc-pro-flasher.AppImage`, `gbsc-pro-flasher.deb`, `gbsc-flasher-linux` |

```sh
python tools/flasher-build.py --mac --out <dir>
```

It needs the Tauri CLI (`cargo install tauri-cli --version '^2' --locked`) and builds the UI first. The cross-build
from macOS needs `rustup target add x86_64-pc-windows-msvc`, `cargo install cargo-xwin` and `brew install llvm lld`.
`tools/release.py --flasher <dir>` puts that folder into the release zip; how releases are built and published is in
[CONTRIBUTING.md](../CONTRIBUTING.md#releases). The `flasher` workflow (`.github/workflows/flasher.yml`) builds the
three systems by hand without publishing anything; it runs the same jobs as a release (`flashers.yml`), so a system
already built for these sources comes from the cache and a run on `main` leaves a release nothing to build.

### The ESP8266 stub

The flasher is free software under the GPL-3.0 (`resources/LICENSES/GPL-3.0.txt`). `esp8266-stub.json` is esptool's
ESP8266 flasher stub, copied unchanged from esptool 4.8.1; it is GPL-2.0-or-later
(`resources/LICENSES/esptool-stub-GPL-2.0.txt`), and its source and checksum are in
[resources/STUB-SOURCE.md](resources/STUB-SOURCE.md).

The stub is not compiled into the flasher: it is a separate file that the flasher reads and sends to the ESP8266's
RAM, where it runs. The app looks for it in its bundled resources, next to its executable and in the folder that
holds the app; the command line next to its executable. Without the file the flasher still
works, through the ESP8266 ROM loader: slower (115200 baud, no compression) and without the MD5 check.

<p align="center">
    <img alt="GBSC PRO" src="assets/gbsc-pro%20logo.png" />
</p>

---

Open-source firmware for the **GBSC PRO** video upscaler: retro consoles in, HDMI out, from 480p to 1200p with low
latency. It also runs on the original gbs-control boards (GBS-8200/8220 with an ESP8266).

The GBSC PRO is a [gbs-control](https://github.com/ramapcsx2/gbs-control) board with an extra decoder for S-Video and
Composite. Its firmware has two parts that are released together:
- **GBS-Control** (ESP8266): the scaler, the TV menu, the OLED, the remote, WiFi and the web app;
- **ADV Controller** (HC32F460 microcontroller): the ADV7280 video decoder and the ADV7391 encoder.

[Releases](https://github.com/brisma/gbsc-pro/releases) · [Changelog](CHANGELOG.md) ·
[Discord](https://discord.com/invite/2MMWRkVRbk) ·
[Report a problem](https://github.com/brisma/gbsc-pro/issues/new/choose)

## What it does

**Inputs and outputs**
- RGB SCART (csync or composite sync), RGB SoG, Component (YPbPr), VGA, and on the GBSC PRO S-Video and Composite
  through the ADV7280 decoder.
- **Automatic input**: with Source on Auto (the default) the GBSC finds the connected source by itself, tells an RGB
  SCART cable from a composite-only one before the first picture, and follows a console swap on the same cable.
- Output 480p/576p, 720p, 960p, 1024p, 1080p, 1200p (experimental) or Downscale; every HD output is 4:3.
- Motion-adaptive deinterlacing, smooth scrolling (frame lock), Always 60 Hz, line doubler (2X) for S-Video and
  Composite.

**Picture**
- Filter (Normal, Sharp, Smooth) and sharpness, scanlines (Light, Medium, Strong), brightness, contrast, saturation,
  colour, HDMI range; decoder brightness, contrast, saturation, hue and filters for S-Video and Composite.
- Position, size and masks per signal mode.

**Menus**
- One menu, the same on the TV and on the OLED, with an **Easy** mode for the everyday items and an **Advanced** mode
  for everything, and a help line for every item.
- **Quick menu** (OK with the menu closed): profile, input, resolution, filter and scanlines in one place.
- Remote control and rotary knob; status pop-up when the signal or the profile changes; four TV themes.

**Profiles**
- Up to 36 named profiles with a region (NTSC-U, NTSC-J, PAL, PAL60), each remembering its signal: the right profile
  loads by itself when that console comes back.

**Web app** (phone, tablet, PC: open `http://gbscontrol.local`)
- The same settings as the TV menu, profiles (export and import), status with the signal history and a console log,
  WiFi, updates, backup and restore, and a remote that drives the TV menu.

**Updates**
- Over the air from the TV menu, the OLED or the web app, on the **Stable** or **Beta** channel (see below).
- The ADV Controller is updated by GBS-Control through the internal link: no USB cable after the first install.
- Safe mode after repeated crashes keeps WiFi, update and backup available.

## Getting started

### Supported hardware

| Board | What to flash |
|-------|---------------|
| GBSC PRO | GBS-Control and the ADV Controller, always from the same release |
| Original gbs-control (GBS-8200/8220 + ESP8266, optional OLED, knob and Si5351) | GBS-Control only. *Not yet tested by users: reports are welcome.* |

### First install (USB)

1. Download `gbsc-pro-<version>.zip` from the [latest release](https://github.com/brisma/gbsc-pro/releases) and unzip
   it. The folder holds the firmware files and the **GBSC PRO Flasher** for macOS, Windows and Linux.
2. Open the flasher from that folder (it preselects the files next to it):
   - macOS: right-click the app, choose Open, then Open again (the app is not signed by Apple);
   - Windows: if SmartScreen warns, choose "More info", then "Run anyway";
   - Linux: `chmod +x gbsc-pro-flasher.AppImage`, and your account needs access to serial ports.
3. **GBS-Control**: connect the GBSC's USB port, choose the device and the `.bin` file, then Flash.
4. **ADV Controller** (GBSC PRO only): hold the ADV Controller's button while you plug its USB-C cable (bootloader
   mode), choose the device and the ADV Controller image, then Flash.
5. Connect to the GBSC's WiFi hotspot and open `http://192.168.4.1`, or set WiFi from the TV menu
   (System > WiFi); from then on the web app is at `http://gbscontrol.local`.

More about the flasher, its command line and backups: [gbsc-pro-flasher](gbsc-pro-flasher/README.md).

### Coming from 2.4.x

Firmware 3.0 is a rewrite. Flash **both** firmwares over USB the first time (the 2.4.x ADV Controller cannot be
updated over the link). Settings and slots are not carried over: 3.0 starts from the defaults and slots become named
profiles. The details are in the [upgrade notes](CHANGELOG.md#upgrade-notes).

### Going back to 2.4.x

Flash both files of the [2.4.1 release](https://github.com/brisma/gbsc-pro/releases/tag/v2.4.1) over USB with the GBSC
PRO Flasher (it asks to confirm files without a version marker) and choose **Erase settings and profiles** for the
ESP8266: 2.4.1 does not read the 3.0 settings. Take a backup first if you plan to return to 3.0.

### Updating

- **System > Update** on the TV, the OLED or the web app: Check now, then Install. The ADV Controller follows: a
  prompt at start-up offers its update when a newer one is built into GBS-Control.
- The web app (System > Update) also takes a firmware file you downloaded, and the flasher can update a unit over
  WiFi.

### Release channels

- **Stable**: the recommended releases.
- **Beta**: release candidates, already tested, waiting for users' feedback before they become Stable. A Beta build
  is offered the newest Beta or Stable; System > Update > Other channels moves a unit from one channel to the other
  or back to an older version.

## Hardware

The GBSC PRO is sold ready to use at the [official store](https://www.aliexpress.com/item/3256808268415575.html): that
board is the one the firmware supports. Its design files (schematic, Gerber, BOM and datasheets) are open, for
reference, in [hardware/](hardware/).

![](assets/product-01.jpg)

## Contributing

Bug reports and ideas: [issues](https://github.com/brisma/gbsc-pro/issues/new/choose) or the
[Discord](https://discord.com/invite/2MMWRkVRbk). For a bug, paste the report of the web app (Status > Diagnostics >
Copy for an issue). Building the firmware, running the tests and the project's layout:
[CONTRIBUTING.md](CONTRIBUTING.md).

## Thanks

- [@ramapcsx2](https://github.com/ramapcsx2) and the contributors of [gbs-control](https://github.com/ramapcsx2/gbs-control),
  the firmware this project builds on.
- [RetroScaler](https://github.com/RetroScaler/gbsc-pro) for the GBSC PRO hardware and its first firmware releases.

## License

GBSC PRO is free software under the [GNU General Public License v3.0](LICENSE), the licence of
[gbs-control](https://github.com/ramapcsx2/gbs-control) it derives from. Third-party code keeps its own licence: the
XHSC HC32F460 drivers and USB device library (BSD 3-Clause, `adv-controller/lib/`), WebSockets and PersWiFiManager
(`gbs-control/3rdparty/`), si5351mcu (LGPL 2.1, `gbs-control/src/`), the PlatformIO libraries (ESPAsyncWebServer,
IRremoteESP8266, ThingPulse OLED driver) and the esptool flasher stub (GPL 2.0 or later,
`gbsc-pro-flasher/resources/LICENSES/`).

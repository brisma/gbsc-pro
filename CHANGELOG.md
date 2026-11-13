# Changelog

All notable changes to the GBSC PRO firmware (GBS-Control on the ESP8266 and the ADV Controller), its web app and
its flasher. Releases 1.0 to 1.3 were published by [RetroScaler](https://github.com/RetroScaler/gbsc-pro); from
1.2.3 ADV CLI on they come from [brisma/gbsc-pro](https://github.com/brisma/gbsc-pro). Issue numbers without a link
refer to brisma/gbsc-pro.

## 3.0.0-beta2

### Fixed
- Profiles: a console name with its region is shown whole in the Load list and in the messages ("SNES/Super Famicom
  NTSC-U" and "Genesis/Mega Drive NTSC-U" lost a letter).
- Backup: a backup downloaded right after a change contains it (it was saved up to two seconds later).

## 3.0.0-beta1 - 2026-11-12

A rewrite of GBS-Control and the ADV Controller firmware on top of upstream gbs-control: one new menu for the TV OSD and
the OLED, automatic input search, named profiles, a new web app, updates over the air by channel and a new flasher. The
same firmware also runs on standard gbs-control boards.

**Update both firmwares together.** GBS-Control 3.0.0 and ADV Controller 3.0.0 only work with each other, and settings
and slots from 2.4.x are not carried over. Read the [upgrade notes](#upgrade-notes) before you flash.

### New features
- One menu for the TV OSD and the OLED, in Easy and Advanced mode, with a help line for every item.
- Quick menu: OK on the remote (or a click of the knob) with the menu closed.
- Source **Auto** (the default) finds the connected input and tells an RGB SCART cable from a composite one.
- Named profiles (up to 36, with regions) replace slots and load by themselves when their signal comes back.
- New web app for phone, tablet and PC, with a remote for the TV/OLED menu.
- Updates over the air by channel (Stable, Beta), from the menu or the web app.
- The ADV Controller is updated by GBS-Control: no USB cable after the first install.
- New GBSC PRO Flasher for macOS, Windows and Linux.

### Menus
- Menu mode (last row of the main menu): Easy shows the common items, Advanced every item. The Developer menu is inside
  Advanced (System > Developer menu).
- Countdown on risky changes (resolution and similar): keep the change or it reverts on its own.
- Status pop-up with the signal and the profile when they change (System > Status pop-up).
- New default OSD theme **GBSC PRO** (black and yellow). Classic, Light and Retro remain; Dark was removed.
- System > OSD settings (Advanced): menu position, pop-up position, theme, auto close.
- On-screen keyboard for profile names, passwords and host names.
- Volume in percent (was dB steps), saved in the profile; mute is one setting for every profile.
- The firmware versions moved to System > Update; the GBS-Control version is also in the menu title.
- The Preferences menu is gone: Theme is in System > OSD settings, Volume and Mute in System > Sound.
- Inputs renamed: RGB SCART (was RGBs), RGB SoG (RGsB), Component (YPbPr), Composite (AV).
- Settings renamed: Sync cleanup (was Sync Stripper), Smooth scrolling (Frame Time Lock), Always 60Hz (Force 50Hz to
  60Hz), HDMI range (HDMI Limited Range), 2X (I2P/2X), Color sharpness (Step response), Position and Size (Move and
  Scale), Edge mask (Borders), PAL full height (Full height), Boot calibration (ADC calibration), External clock (Clock
  generator).
- Matched presets is now the Resolution choice 960p/1024p; 960p fixed and 1024p fixed are in Advanced.
- The decoder settings (comb, filters, adaptive contrast) are in Input > Decoder (Advanced), with plainer names.
- The product name is written GBSC PRO everywhere.

### Remote and OLED
- OK opens the Quick menu, MENU the full menu. With no signal the Quick menu picks the input.
- Knob: a long press closes the Quick menu; its last row, All settings, opens the full menu.
- Knob: System > Knob direction to reverse it, and Knob steps (Advanced: 1, 2 or 4 menu steps per click) for encoders
  that skip or double steps.
- OLED: new boot splash, home screen (source and output, rates, profile, IP, volume, WiFi bars) and a screensaver after
  3 minutes.

### Input
- Source **Auto** tries VGA, then RGB SCART / RGB SoG / Component, then S-Video, then Composite. The input found last is
  tried first at the next start.
- A manual Source returns to Auto when its signal goes away, after Input > Wait before search (Advanced, default 2 s).
- Input > Auto search: the inputs the automatic search looks at.
- RGB SCART or Composite is decided from the SCART cable before the first picture, also on a black screen. A hint
  suggests Composite when an RGB SCART picture stays black for 20 s.
- Swapping consoles on the same cable (a 50 <-> 60 Hz change, or the picture back after a gap) starts a quick search.
- Input > Adaptive Sync (on by default, saved in the profile): a steady picture on bright screens with composite sync.
- Input > 75 ohm sync (Advanced, RGB SCART only, not saved) for luma or composite sync cables. *Experimental*. (#7, #31)

### Picture
- **720p is now 4:3** with side borders (NTSC and PAL), like 1080p.
- **1200p output (1920x1200)** at 60 Hz NTSC and 50 Hz PAL. *Experimental*: not yet tested on a 16:10 monitor. (#35)
- Picture > Filter: Normal, Sharp, Smooth. Sharpness from 0 to 31 (Advanced) replaces the Sharpness and Peaking
  switches. The line filter is now on by default.
- Picture > Scanlines: Off, Light, Medium, Strong; Scanline strength is in Advanced.
- Picture > Brightness, Contrast and Saturation (0 to 100, 50 neutral) work on every input; they replace Y gain, Color
  and the web app's colour correction.
- Decoder Brightness, Contrast, Saturation and Hue (S-Video, Composite) now go from 0 to 100, 50 neutral.
- Output > HDMI range: Auto (was HD, still the default), Full (was Off), Limited (was All), Limited SD (was SD;
  Advanced).
- One output change per 50 <-> 60 Hz switch instead of two.
- Output > Smooth scrolling (frame lock) averages the input rate: less output rate jitter.
- Output > Always 60Hz applies at once, without a restart, and the external clock no longer pulls the output back to 50
  Hz.
- 2X turns itself off with a message on sources the scaler cannot lock (progressive 50 Hz).
- Screen > Mask position moves the masked window without resizing it (Advanced). (#27)
- Position, Size and Developer tweaks are saved per signal mode instead of per PAL/NTSC group. (#45, #52)
- Output > Analog out (RGB / YPbPr, Advanced) and Input > VGA upscaling are available again.
- The external clock generator (Si5351) is reset with the datasheet sequence and waits for lock.

### WiFi
- Optimize WiFi (System > WiFi, Advanced, on by default): at power-on the firmware picks the scaler memory clock that
  disturbs the WiFi least.
- WiFi no longer slows down while no source is connected; the radio sleeps while nobody uses the web app.
- The WiFi page of the web app says when the picture disturbs the router's channel and suggests another one.
- WiFi stays connected with routers that keep announcing a channel change they never make.

### Profiles
- Up to 36 named profiles, each with a region (NTSC-U, NTSC-J, PAL, PAL60), replace slots. Names are unique per region.
- Profiles > Auto-load (on by default): a profile loads when the signal it was made for comes back, also when you swap
  consoles.
- A profile is tied to its console's own line rate too: on RGB SCART, RGB SoG and Component, two consoles with the same
  standard (a Mega Drive and a SNES in 240p) each get their own profile. A console with no profile of its own starts
  from Defaults.
- Profiles page on the OSD and OLED: Defaults, New profile, Save changes, Discard changes, Manage (Rename, Replace with
  current, Delete), Startup.
- Discard changes loads the profile in use again as it was saved.
- Save changes keeps the profile's signal; Manage > Replace with current moves it.
- Loading a profile by hand also sets its input and resolution (Profiles > Load sets input, on by default). (#22)
- Startup = Defaults keeps the last output resolution.
- Web app: export and import of single profiles; the full backup includes all profiles. (#22)

### Web app
- New web app: Quick, Settings (Easy/Advanced, the same items as the TV menu), Profiles, Status (signal, history,
  console log) and System (WiFi, updates, backup and restore).
- Works on phone, tablet and PC; it can be added to the phone's Home Screen.
- Remote for the TV/OLED menu.
- Developer mode: register tools for the scaler and the ADV Controller (they replace Custom I2C).
- Pro-only controls are hidden on boards that lack the hardware.
- Requests are queued: fast clicking or several browsers no longer lock up or crash the ESP8266. (#47)

### Updates
- OTA channels in System > Update: Check now, Install, Other channels (Stable, Beta). With Auto check (Advanced, on by
  default) the unit looks for new firmware once a day and reports it; nothing is installed by itself. Updates download
  over plain HTTP from gbsc.brisma.it.
- Firmware upload (gzip or raw) from the web app, with a question before a downgrade.
- Safe mode after three crash resets in a row: WiFi, firmware upload and backup stay available.
- Arduino upload (was Enable OTA) is in the Developer menu and needs a password.

### Tools
- GBSC PRO Flasher: a new desktop app for macOS, Windows and Linux, with a command-line version. It replaces the Python
  flasher.
- Over USB it flashes the ESP8266 (with optional wipe) and the ADV Controller; over WiFi it updates both firmwares and
  backs up and restores settings and profiles.
- Windows: allow the Firewall prompt at the first "Find on the network", or the search cannot see the GBSC.

### ADV Controller
- New firmware 3.0.0 with a new link: GBS-Control sends the full state and checks that it was applied.
- Updated through GBS-Control (prompt at boot, System > Update > ADV Controller, web app). A power cut during the upload
  keeps the old firmware.
- Reports the decoder signal: the menus show the region on S-Video/Composite, and "No signal" over the free-run colour
  bars.
- Noise reduction and colour transient improvement are off by default, with switches in Input > Decoder > Filters
  (Advanced). (#56)
- Input > Video standard: **Auto 7.5 IRE** for NTSC consoles whose black looks grey. (#46)
- The VBI lines are opened at every start: the top lines of C64/Atari and similar sources are no longer cut. (#39)
- The USB-C port shows a console (info, update, log) while the firmware runs.
- The ADV Controller no longer stores settings: its flash is written only by a firmware update.

### Standard gbs-control boards
*Untested by users so far: reports welcome.*
- The same GBS-Control firmware runs on the original gbs-control (GBS-8200/8220 with an ESP8266; optional OLED, rotary
  encoder and Si5351). Flash only the ESP8266. (#54, #18)
- The board is detected at start or set in System > Board type (Advanced).
- Auto input works on the standard board too.
- Downscale and HD bypass outputs on these boards.
- Upstream's TV5725 icon OSD is available on these boards (Quick menu > TV OSD, System > Changes on TV).

### Source code
- The whole repository is GPL-3.0, the licence of upstream gbs-control (it was MIT since 2.3.0).
- Builds and tests run the same way on Windows, macOS and Linux, natively or in a development container; every push is
  tested on all three systems.

### Fixed
- #22 Export presets / per-preset output.
- #27 Move controls for borders.
- #35 1200p output (experimental).
- #39 C64 top of the picture missing (waiting for a confirmation).
- #46 S-Video "true black" (waiting for a confirmation).
- #47 Web timeouts when clicking fast.
- #50 Resolution fine-tuning: Edge mask and Mask position, saved per profile and mode.
- #54 Compatibility with other GBSC boards (to be confirmed by users).
- #56 ADV DNR and CTI on by default.
- Picture shaking on bright screens with composite sync (seen on a Mega Drive).
- Probably fixed, waiting for a test: #45 PS2 progressive scan glitch, #52 green screen after 2.4.1, #18 firmware for
  the blue GBS-C.
- Partly addressed: #3 MVS (Sync watcher Off is saved in the profile), #7 PS1 sync problems and #31 RCA on SYNC (75 ohm
  sync), #41 OSD distortion (new OSD driver), #49 4:3 letterbox (720p and 1200p are now 4:3), #51 WiFi stops at 1080p
  (Optimize WiFi), #63 elevated black levels (Picture > Brightness), #65 PAL PSX signal loss (one output change per
  50/60 Hz switch).

### Known limits
- 1200p output and 75 ohm sync are experimental.
- Standard gbs-control boards have not been tested by users yet.
- Adaptive Sync is experimental: if a source loses sync or its picture shakes, turn it off (Input > Adaptive Sync) and
  report the console and the cable.
- Auto-load cannot tell two consoles apart when they share their timing (a NES and a SNES; a PlayStation, an N64 and a
  Saturn), nor any two on S-Video or Composite: the profile used last loads, pick the other one in Profiles > Load.
- Some output resolutions disturb one 2.4 GHz WiFi channel with some sources (seen: a PAL N64 at 960p/1024p with the
  router on channel 2): set a fixed channel (1, 6 or 11) on the router.
- No 4:3 / 16:9 choice: every HD output is 4:3, so 720p no longer fills a 16:9 screen. (#66)
- No CEA 720x576 preset for 576i/50 Hz. (#17)
- The Linux flasher has not been tried on a Linux PC with a GBSC yet.
- 2X on interlaced sources (480i/576i games, e.g. N64 hi-res modes) may flicker: keep 2X off for those games.
- The AV jack and SCART pin 20 are the same input: plug one cable at a time, or the black of an RGB SCART console turns
  slightly green.
- The composite of an RGB SCART cable can show wrong or flickering colours, or a too strong picture: with an RGB cable,
  use RGB SCART.

### Upgrade notes
- Flash GBS-Control and the ADV Controller from the same release. The first time, from 2.4.1, flash both over USB with
  the GBSC PRO Flasher: the 2.4.1 ADV Controller cannot be updated over the link.
- Settings and slots from 2.4.1 are not carried over: 3.0.0 starts from the defaults and removes the old slot files.
  Recreate your slots as profiles. Backups made with 2.4.x cannot be restored.
- Standard gbs-control boards: flash only the ESP8266.
- To go back to 2.4.1, flash both 2.4.1 files over USB with the GBSC PRO Flasher and erase the settings and profiles:
  2.4.1 does not read them.

## 2.4.1 - 2026-05-02

Bug fixes.

### Changed
- Keep HDMI Output removed: it caused more problems than it solved.
- Profiles that are no longer compatible are deleted automatically.
- The developer settings for PAL and NTSC are kept apart.

## 2.4.0 - 2026-04-25

Keep output on no signal, new S-Video/Composite filters and Hue, per-slot developer values, and a safer backup
format.

### Picture
- Video filters on S-Video/Composite: Luma Mode, Chroma Mode, Chroma Taps.
- Hue on S-Video/Composite.
- Keep Output on No Signal: the HDMI link stays on with a black screen when no input is present (OSD, OLED System
  Settings, web app).
- The PLL divider can be decreased as well as increased.

### Slots
- Developer menu values (HTotal, PLL divider, SDRAM clock, ADC filter, OSR, SOG level, Sync Invert), Screen
  Move/Scale and the SyncWatcher state (inherit / force on / force off) are saved per slot.
- Backups use a versioned format with a CRC32 check; slot uploads are checked for size.

### Remote and OLED
- IR repeat accelerates on a long hold (2x after 2.5 s, 4x after 4 s) for menu and volume.

### ADV Controller
- The encoder is reconfigured when the video format (PAL/NTSC) changes after start.

### Web app
- The firmware version is taken from the firmware header at build time.

### Fixed
- Factory Reset restores the right input (RGBs).
- Saturation on S-Video/Composite.
- Web Color Correction mapping: Contrast, Pb/U gain and Pr/V gain act on the right registers.
- SDRAM/PLL clock persistence.

## 2.3.0 - 2026-01-10

Sync Stripper control, HDMI Limited Range, crash recovery for the ADV Controller and a new flashing tool.

### Picture
- HDMI Limited Range compensation against washed-out colours: Off, HD (720p/1080p), SD (480p/576p/960p/1024p), All
  (OSD System Settings, web app with a live status).
- Sync Stripper (formerly Compatibility Mode) controls the LM1881 sync separator on RGB sources; default on (OSD,
  OLED, web app ADC calibration section).

### ADV Controller
- Watchdog (about 2.7 s) for automatic recovery from hangs.
- Fault handlers with LED indication and automatic reset.
- ASW2 polarity fixed: on = LM1881 enabled, off = bypassed.

### Remote and OLED
- Changing the volume from the IR remote unmutes the audio.
- The presets menu returns properly when no presets are found.

### Tools
- GBSC Pro Flasher: a cross-platform GUI/CLI tool that flashes both the ESP8266 and the ADV Controller.
- The web app development server was updated for local testing.

### Source code
- Repository reorganized: gbs-control/ and adv-controller/ at the root, hardware/ with BOM, Gerber files and
  datasheets (including the English HC32F460 datasheet).
- MIT licence added.
- Factory reset deletes all slots and preset files.

## 2.2.0 - 2026-01-07

Shaping and comb filters for S-Video/Composite, buffered link packets and reliable presets.

### Picture
- Shaping Filter and Comb Filter for S-Video and Composite (OSD Video Filters, web app), to adjust the picture
  focus.

### Menus
- The OSD Info Screen was redesigned and shows the input and output signals correctly.
- The OSD preset load/save menu was redesigned and the menu items were reorganized.
- OLED: firmware version item in the main menu (GBS-Control and ADV Controller); all text redrawn in Arial.
- Web app: Resolutions moved to the first section.

### ADV Controller
- Packets received from the ESP8266 are buffered.

### Fixed
- Loading and saving presets.
- Loading and saving the ADC gain.
- No HDMI signal after switching between PAL and EDTV.

## 2.1.0 beta - 2026-01-03

Adaptive Contrast Enhancement and several fixes.

### Picture
- ACE (Adaptive Contrast Enhancement) on the ADV Controller: Luma Gain, Chroma Gain, Chroma Max, Gamma Gain,
  Response Speed (OSD ACE Settings menu, web app).
- ADV Controller values are kept temporarily even without saving or loading a preset.

### Menus
- OLED: up to 36 presets through a virtual menu (less RAM), I2P in the OLED menu.

### GBS-Control
- ESPAsyncWebServer library updated.
- All serial messages go to the WebSocket console.

### ADV Controller
- Faster packet transfer.
- Echo works again after the move to GCC; CM_UART3 connection notes corrected.

### Fixed
- Presets and resolutions applied after a PAL <-> NTSC change.
- RGB <-> YUV colour conversion, RGB colours loaded from presets, RGB values reset on an input change, YPbPr
  default colours.
- OSD Y Gain / Color not updating.

## 2.0.0 beta - 2025-12-30

The code base inherited from RetroScaler was rewritten, which opens the way for new menus and functions.

### Menus
- OSD themes: Classic, Dark, Light, Retro; a Preferences menu with Theme, Volume and Mute.
- Firmware versions of GBS-Control and the ADV Controller in a menu; Restart and Developer entries in the main menu
  (SDRAM clock, PLL divider, Phase/SOG, oversampling...).
- Confirmation before a Factory Reset.
- Menus wrap around at both ends.
- Pages reorganized: R/G/B, then ADC / Scanlines / Line filter, then Sharpness / Peaking / Step response, then Y
  Gain / Color / Default; Y Gain and Color (U/V) controls back with their values.

### Presets
- All ADV Controller and GBS-Control settings are saved per preset; up to 36 presets from the web and the OSD.
- Deleting a preset deletes that slot only, without shifting the others; less memory used to load and save.

### Remote
- Holding a button repeats (volume, RGB, ADC gain, Y gain, colour, move, scale, borders, brightness, contrast,
  saturation).
- MENU always closes the OSD; LEFT/RIGHT toggle on/off settings like OK; EXIT goes back one menu; the first volume
  press applies at once.

### ADV Controller
- Firmware rewritten without the Keil dependency.
- Custom I2C commands for the ADV7280/ADV7391 from the web app (debugging).
- Settings are written to flash at most every 3 seconds instead of at every change.

### Web app
- Controls the Pro hardware does not support were removed (Component output, RGBHV scaling, downscale, bypass
  modes).

### Fixed
- Flicker on Volume, Frame Rate and AV Format; a black glitch when closing the OSD; the missing cursor when opening
  the menu; PAL Hanover bars; S-Video comb control; mute not saved; slot parsing of reserved fields.

### Source code
- Menu and OSD architecture and the OSD driver rewritten; the Pro code moved to pro/.

## 1.4.1 - 2025-12-14

Scanline logic, input handling in the web app and a correct state at power-on.

### Menus
- OSD values refresh when an option changes.
- Scanlines rewritten: the OSD and OLED show when they apply.
- Peaking shows LOCKED while Sharpness is on.

### Web app
- Names and icons for GBSC PRO; input selection (RGB, Composite, S-Video...); forced mode, 2X and Smooth on
  Composite/S-Video; Sharpness on/off.
- A small development server tests the web app without flashing the device.

### Fixed
- The settings of the last input are applied at power-on: no need to select the input again.
- The packet handling towards the ADV7280/ADV7391 was refactored; various minor fixes.

## 1.4.0 "Hope" - 2025-12-10

More refactoring, gbs-control-nx 1.2.3 changes and colour fixes.

### Picture
- RGB colours follow ITU-R BT.601 (no more saturated, reddish colours).
- 480p/576p resolutions in the OSD.
- The 12 dB volume attenuation is gone: 50 is the real maximum.

### Fixed
- Presets corrupted when reaching 72 slots.
- Volume not saved.
- Various minor fixes.

### Source code
- The Pro code lives in its own files (gbs-control-pro.cpp/h), easier to align with upstream.
- Changes from gbs-control-nx 1.2.3 imported.

## 1.3.0 "Reforged" - 2025-12-06

Refactoring on the official gbs-control sources, with separate firmware for the AV module and GBS-Control. Erasing
the flash and a factory reset are recommended.

### GBS-Control
- Based on the official gbs-control sources and on gbs-control-nx by Kwakx, with the latest upstream fixes and the
  WiFi and file system improvements of [gbs-control-nx 1.1.0](https://github.com/Kwakx/gbs-control-nx/releases/tag/v1.1.0).
- Developer console messages restored.

### AV module
- Based on 1.3: UART debugging, ADV CLI, automatic 525p/625p switch (fixes the 1X crop).

## 1.3 - 2025-11-15

Published by RetroScaler.

### AV module
- Automatic 525p/625p switch, fixing the 1X crop
  ([RetroScaler#18](https://github.com/RetroScaler/gbsc-pro/issues/18)), contributed by brisma.

## 1.2.3 ADV CLI autoswitch - 2025-11-01

Experimental AV module firmware based on 1.2.3: UART debugging, ADV CLI, automatic 525p/625p switch (fixes the 1X
crop).

## 1.2.3 ADV CLI - 2025-10-30

First experimental AV module firmware with ADV CLI support, based on 1.2.3.

## 1.2.3 - 2025-07-29

Published by RetroScaler.

### Added
- 1X mode back.
- One-click restore of the AV module defaults, colour included.

## 1.2 - 2025-04-24

Published by RetroScaler.

### Added
- Separate brightness, contrast and saturation on the AV module (OK saves).

### Fixed
- Green screen in PAL mode ([RetroScaler#7](https://github.com/RetroScaler/gbsc-pro/issues/7)).
- More accurate encoder knob.

### Removed
- 1X mode.

## 1.1r - 2025-03-30

Published by RetroScaler. Units bought after 25 March 2025 already have it.

### Added
- Input resolution recognition from 480i to 576p.

### Fixed
- Inaccurate recognition of the AV module.
- Knob menu switching updated.

## 1.1 - 2025-03-06

Published by RetroScaler (ESP8266 firmware only).

### Fixed
- OLED occasionally not starting.
- OLED occasionally going black when using IR.
- Crackling sound at high volume.

## 1.0 - 2025-01-03

Published by RetroScaler.

### Changed
- The YUV colour channel adjustment is replaced by RGB channel adjustment.

### Removed
- The OSD function of the OLED menu.

### Fixed
- Blurred picture on some consoles in line double (1X) mode.

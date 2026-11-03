# ADV Controller

Firmware of the ADV Controller, the HC32F460 microcontroller of the GBSC PRO that drives the ADV7280 video decoder
(S-Video and Composite inputs) and the ADV7391 video encoder. It is released together with GBS-Control, the ESP8266
firmware: see the [main README](../README.md) for users and [CONTRIBUTING.md](../CONTRIBUTING.md) for the build
environment, the tests of the whole repository and the release process.

## What it does

- **Applies the state GBS-Control sends.** GBS-Control sends the full ADV state (input, video standard, decoder
  picture settings, filters, line doubler) over a UART link. The ADV Controller works out which register groups
  changed and writes only those to the ADV7280 and ADV7391 over I2C. It keeps no settings of its own: until the first
  state arrives it holds the RGB SCART path with the decoder powered down.
- **Switches the inputs.** It sets the four analog input switches and the AV connect line for each input, and powers
  the decoder up only when it is needed (S-Video, Composite, or RGB SCART while the input search checks the SCART
  cable's composite line).
- **Follows the decoder with the encoder.** The ADV7391 outputs 525p or 625p YPbPr, following the decoder's output
  rate; the ADV Controller changes it by itself.
- **Reports the decoder signal.** Every 50 ms it reads the ADV7280 status registers; GBS-Control asks for the
  debounced result (lock, free run, standard, 50/60 Hz, interlaced) and shows it in its menus.
- **Updates itself over the link.** GBS-Control carries an ADV Controller image and sends it over the same link; the
  new image is staged in flash and verified before it replaces the running one.
- **Offers a console** on its USB-C port: register access, link and update status, a log of link events.
- **Shows the input on the status LED**: one colour per input, red while the decoder runs free (no lock).

## Code layout

| Path | What it holds |
|------|---------------|
| `src/main.c` | Start-up (clocks, pins, safe input path), then the main loop: link poll, decoder status every 50 ms, LED every 100 ms, console; the core sleeps (`WFI`) until the next interrupt |
| `src/link.c`, `link.h` | Link dispatcher: parses frames, applies STATE as a diff, answers requests, runs the update session. No vendor driver code, so it builds and is tested on the host |
| `src/link_hal.h` | The hardware interface `link.c` calls |
| `src/link_hal_adv.c` | Its video half: decoder and encoder groups, input switches, I2C access, the signal report |
| `src/link_hal_hc32.c` | Its chip half: time base, INFO, replies on the shared wire, staging flash erase and program, reading the RX ring |
| `src/uart_dma.c` | USART4 receive on PB7 at 115200 baud into a 4 KB DMA ring |
| `src/adv7280.c` | ADV7280 register tables: init, input (S-Video or Composite), video standard, I2P (2X) and smoothing, adaptive contrast, brightness/contrast/saturation/hue, filters, the periodic status read |
| `src/adv7391.c` | ADV7391: 525p/625p mode following the decoder, retried on its own after a failed write |
| `src/board.c` | GPIO: analog switches ASW1-4, AV connect, decoder power and reset, encoder output enable |
| `src/i2c.c` | I2C master for both chips |
| `src/signal.c` | Signal report: debounced fields from the status reads (no extra I2C) |
| `src/led.c` | Status LED |
| `src/console.c`, `console_fmt.c` | Console core (line editing, commands, register dumps, log) and its texts; host-tested |
| `src/console_hc32.c`, `debug_uart.c` | Console glue on the chip and the USART3 debug port |
| `src/usb_console.c`, `usb_cdc.c`, `usb_dev_desc.c`, `usb_bsp.c`, `usb_guard.c`, `cdc_ring.c` | USB CDC console: port, CDC-ACM class, descriptors, board support, safe start and interrupt-storm guard, TX/RX rings |
| `src/fw_header.c` | Image header (see below) |
| `src/fw_copy.c` | RAM routine that copies a verified staged image over the application and resets |
| `src/fault_handler.c` | Fault handlers and the watchdog (about 2.7 s) |
| `src/hc32f4xx_conf.h`, `usb_app_conf.h` | Driver and USB configuration |
| `lib/` | Vendor code, each file under its own licence: Arm CMSIS headers, and from XHSC the HC32F460 device files, low-level drivers, board support package and USB device core |
| `linker/hc32f460-app.ld` | Linker script: application at 0x10000, header at 0x10400 |
| `tools/fw-header.py` | Writes and checks the image header |
| `tools/check-ramfunc.py` | Post-link check of the RAM copy routine |
| `test/` | Host tests and golden I2C traces |
| `release/adv-controller.bin` | The committed release image that GBS-Control embeds |
| `VERSION` | Firmware version |

### Hardware interfaces

| Interface | Pins | Use |
|-----------|------|-----|
| I2C1, 200 kHz | PA6 SCL, PA7 SDA | ADV7280 at 0x42 (its VPP map at 0x84), ADV7391 at 0x56 (8-bit write addresses) |
| USART4 | PB7 | The link to GBS-Control (one wire, both directions) |
| USART3, 115200 baud | PH2 TX, PC13 RX | Debug console |
| USB full speed | PA11, PA12 | USB-C console |
| GPIO | PB15-PB12 (ASW1-4), PA8 (AV connect), PB0 (power down), PA3 (ADV7280 reset), PB1 (ADV7391 output enable) | Input switches and video chips |

## The link

The protocol is shared C code used by both firmwares: [`shared/adv-link/adv-link.h`](../shared/adv-link/adv-link.h)
and `adv-link.c` (frames, parser, state, diff planner, replies, update messages). GBS-Control's side is in
`gbs-control/pro/link/`, and the shared code is also covered by GBS-Control's native tests
(`gbs-control/test/test_adv_*`).

- **Wire.** One line, half duplex. GBS-Control sends at 115200 baud; the ADV Controller answers on the same line at
  38400 baud, switching PB7 to transmit only for the reply. A request is answered only while GBS-Control is still
  listening (a short window after the request); a late reply is dropped and counted.
- **Frame.** `0xA7 0x3C | len | seq | cmd | payload (up to 96 bytes) | CRC16` (CCITT-FALSE, big-endian, over len
  to payload). Bit `0x40` of `cmd` asks for a reply, bit `0x80` marks a reply.

| Command | Code | What it does |
|---------|------|--------------|
| `STATE` | 0x01 | The full ADV state (40 bytes, versioned, with its own CRC8). Only the newest one is applied, as a diff: input, format, switches, I2P, ACE, BCSH and filter groups. A group whose I2C write failed is applied again with the next STATE |
| `I2C_BATCH` | 0x02 | Up to 30 raw register writes (the web app's Developer tools) |
| `INFO` | 0x43 | Firmware version, build id, decoder flags, uptime, frame counters and a hash of the applied state: GBS-Control resends the state when the hash does not match its own |
| `PING` | 0x47 | Protocol version |
| `SIGNAL` | 0x49 | Signal report: present, lock, free run, standard, 50/60 Hz, interlaced, the raw status bytes, encoder mode |
| `REG_READ` | 0x4A | Up to 32 registers of one chip or map (Developer tools) |
| `UPD_BEGIN`, `UPD_DATA`, `UPD_COMMIT`, `UPD_STATUS` | 0x44-0x46, 0x48 | Firmware update |
| `NAK` | 0xFF | Reply to an unknown request |

`PING`, `INFO`, `NAK` and the four update messages keep their format in every later version, so any future GBS-Control
can still identify and update this firmware.

## Flash layout and image

The HC32F460 has 512 KB of flash in 8 KB sectors.

| Address | Size | Content |
|---------|------|---------|
| 0x00000-0x0FFFF | 64 KB | Bootloader HCMGBoot (the vendor's USB bootloader, not built from this repository) |
| 0x10000-0x3FFFF | 192 KB | Application (this firmware); image header at 0x10400 |
| 0x40000-0x6FFFF | 192 KB | Update staging area (24 sectors) |
| 0x7C000 | 8 KB | Settings record of older firmware: erased once at start-up if present, unused otherwise |

The image header sits right after the vector table, at offset 0x400 of the image:

| Field | Type | Value |
|-------|------|-------|
| magic | 4 bytes | `ADVC` |
| header version | u16 | 1 |
| reserved | u16 | 0 |
| version | char[12] | the contents of `VERSION`, NUL-terminated |
| image length | u32 | size of the `.bin` |
| image CRC32 | u32 | zlib CRC32 of the whole image with this field set to zero; also the build id reported in INFO |

The build writes the length and the CRC32 into the `.bin` after linking (`tools/fw-header.py --patch`);
`tools/fw-header.py --check <bin>` verifies an image.

### Update over the link

1. `UPD_BEGIN` gives the size, CRC32 and version. The ADV Controller erases the staging sectors one per main-loop pass,
   never while a frame is arriving.
2. `UPD_DATA` writes 64-byte chunks; every reply carries the next offset expected, so an interrupted transfer
   resumes where it stopped. The running CRC32 is computed from what the flash holds.
3. `UPD_COMMIT` succeeds only when the image is complete, its CRC32 matches and the staged header, initial stack
   pointer and reset vector are valid. Once GBS-Control has the reply, the copy routine runs from RAM with interrupts
   off: it erases the application's first sector, copies and verifies every other sector, writes the first sector
   last and resets.

Until the copy starts, the running firmware is untouched: a power cut during the transfer keeps it. If the copy
itself is interrupted, the first sector stays erased, so the bootloader never jumps into a half-written image; the
image is then written again over USB. Flash programming stays locked at run time and is opened only around the
staging writes and the copy. `tools/check-ramfunc.py` checks after every link that the copy routine calls nothing
outside itself and only uses its own RAM buffer.

GBS-Control offers the update itself (a prompt at start-up when it carries a newer image, System > Update >
ADV Controller, the web app); the GBSC PRO Flasher can start it over WiFi with `gbsc-flasher update-adv --host <host>`.

## USB console

While the firmware runs, the ADV Controller's USB-C port is a USB serial port (CDC-ACM, VID:PID 1209:0001, product
"ADV Controller console"). Open it with any serial terminal; output starts once the terminal opens the port (DTR).
The same console is on the debug UART (USART3, 115200 baud). Holding the button while plugging the cable starts the
bootloader instead, which is a different USB device.

| Command | What it does |
|---------|--------------|
| `r AA RR` | Read register `RR` of the chip at 8-bit address `AA` (for example `42` ADV7280, `56` ADV7391) |
| `w AA RR VV` | Write `VV` to register `RR` |
| `d AA RR QQ` | Dump `QQ` registers from `RR` |
| `rm`, `wm`, `dm AA MM ...` | The same in an ADV7280 sub-map `MM` (`00` main, `20` interrupt/VDP, `40` map 2, `80` VPP) |
| `info` | Version, build id, uptime, link counters, applied state hash, input and lock, USB statistics |
| `update` | Update session state and progress |
| `log on`, `log off` | Show link frames and applied states as they happen (rate-limited) |
| `help` | List the commands |

Values are hexadecimal. Register writes from the console are not known to GBS-Control: the next STATE that touches
the same group overwrites them.

## Building

Tools: the Arm GNU Toolchain **15.3.rel2** (`arm-none-eabi-gcc -dumpversion` prints `15.3.1`), GNU make and Python 3.
The version is pinned because the image depends on it: `python tools/dev.py doctor` flags any other one. How to
install them, or use the development container instead, is in
[CONTRIBUTING.md](../CONTRIBUTING.md#building-and-testing).

From the repository root:

| Command | Result |
|---------|--------|
| `python tools/dev.py build --adv` | Release build, copied to `out/adv-controller.bin` |
| `python tools/dev.py build` | Both firmwares; GBS-Control embeds that local ADV Controller image |
| `python tools/dev.py build --esp` | GBS-Control only; it embeds the committed `release/adv-controller.bin` |

In `adv-controller/`:

| Target | What it does |
|--------|--------------|
| `make` | Release build (`-Os`) in `build/release/` |
| `make debug` | Debug build (`-O0 -g3`, `__DEBUG`) in `build/debug/` |
| `make release` | Release build, copied to `release/adv-controller.bin`, header checked |
| `make test` | Host tests and golden traces (below) |
| `make golden` | Golden traces only |
| `make size` | Section sizes of the ELF |
| `make disasm` | Disassembly with source in `build/<BUILD>/adv-controller.dis` |
| `make clean` | Removes `build/` |
| `make help` | Lists the targets |

Variables: `BUILD=debug|release`, `CROSS_COMPILE` (default `arm-none-eabi-`), `PYTHON`, `HOSTCC` (host compiler for
the tests, default `cc`).

Each build writes `adv-controller.elf`, `.bin` (the flashable image, with its header patched), `.hex` (the same image
placed at 0x10000) and `.map`. With 15.3.rel2, a build on macOS or Linux reproduces `release/adv-controller.bin` byte
for byte; the Windows package of the same toolchain links a 4-byte longer `_fini`, so a Windows build differs by those
bytes. Releases always use the committed image.

## Testing

`make test` (or `python tools/dev.py check adv` from the root) builds the tests with the host C compiler and runs
them; the Arm toolchain is not needed.

| Test | What it covers |
|------|----------------|
| `test/test_link.c` (with `stub_hal.c`) | The link dispatcher with the shared protocol code: state diffs and retries, the newest-state rule, replies and their timing window, NAK, register reads, the RGB SCART cable check, the whole update session (resume, bad CRC or header, commit rules, no copy when the reply was lost) |
| `test/test_signal.c` | Signal report: fields, debounce, change counter, decoder off |
| `test/test_i2c.c` | `src/i2c.c` against a fake I2C peripheral, including a stuck bus |
| `test/test_console.c`, `test_console_fmt.c` | Console commands, register output format, dumps, ports, log rate limit, texts |
| `test/test_cdc_ring.c`, `test_usb_guard.c` | USB console rings, USB start sequence and storm guard |
| `tools/test_check_ramfunc.py` | The branch scan of `check-ramfunc.py` |
| `test/test_golden.c` | Golden I2C traces |

The golden trace test compiles the firmware's own video code (`adv7280.c`, `adv7391.c`, `board.c`, `led.c`,
`signal.c`, `link_hal_adv.c`, `link.c`) on the host, with shims for the vendor drivers in `test/shim/`. It records
every I2C write, GPIO change and delay of each path and compares it with `test/golden/*.txt`: start-up on each input
(`boot_rgbs`, `boot_rgsb`, `boot_vga`, `boot_ypbpr`, `boot_svideo`, `boot_composite`), input changes, the Composite
groups, the RGB SCART cable check (`rgbs_listen`, `rgbs_listen_to_av`), the status read loop and an I2C batch. Any
change in the I2C traffic fails it. After a deliberate change, `GOLDEN_UPDATE=1 make golden` rewrites the files;
review their diff before committing.

## Release image

- `VERSION` holds the firmware version (at most 11 characters). It goes into the image header and is what INFO, the
  console and GBS-Control's menus show.
- `make release` builds the image and copies it to `release/adv-controller.bin`, which is committed. A new ADV
  Controller version is a new `VERSION`, a `make release` and both files in the same commit, with a line in the
  changelog.
- GBS-Control embeds the committed image when it is built (`gbs-control/tools/adv-controller-image.py`, a PlatformIO
  pre-script that checks the header and generates `gbs-control/pro/link/adv-controller-image.gen.h`) and offers it
  to the ADV Controller over the link.
- `tools/release.py` refuses a release when the image's version differs from `VERSION` or when the GBS-Control
  firmware does not carry the committed image. The release zip ships it as `adv-controller-<version>.bin` for USB
  flashing.

## Flashing over USB

The first install, a unit coming from 2.4.x, and a recovery after an interrupted copy go through the bootloader over
USB:

1. Hold the ADV Controller's button while you plug its USB-C cable: HCMGBoot starts and shows up as a serial port.
2. In the [GBSC PRO Flasher](../gbsc-pro-flasher/README.md), choose that device and the ADV Controller image, then
   Flash. On the command line: `gbsc-flasher flash adv-controller-<version>.bin --port <port>`; from a checkout,
   `python tools/dev.py flash --adv` flashes `out/adv-controller.bin` and asks you to hold the button first.
3. The bootloader returns no checksum of what it wrote: check the version afterwards in System > Update on the GBSC,
   or with the flasher's check over WiFi.

From then on, updates come from GBS-Control over the link.

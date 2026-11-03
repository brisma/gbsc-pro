# GBSC PRO hardware

Design files of the GBSC PRO board, revision v1.1, made with KiCad: schematic, Gerber files, bill of materials and the
datasheets of the main chips. The firmware and how to install it are in the [main README](../README.md).

## Contents

| File | What it is |
|------|------------|
| [GBSC-AV-IR-v1.1-20240923.pdf](GBSC-AV-IR-v1.1-20240923.pdf) | Schematic, 13 sheets |
| [gerber/GBSC-AV-IR-v1.1-20240923.zip](gerber/GBSC-AV-IR-v1.1-20240923.zip) | Gerber and drill files for the PCB maker: two copper layers, solder mask, paste mask and silkscreen on both sides, board outline, plated and non-plated drill files with their maps |
| [bom/GBSC-AV-IR-v1.1-20240923.xlsx](bom/GBSC-AV-IR-v1.1-20240923.xlsx) | Bill of materials |
| [bom/ibom.html](bom/ibom.html) | Interactive BOM: open it in a browser to see where each part goes on the board |
| [datasheets/](datasheets/) | Datasheets (below) |

The BOM's column headings are in Chinese: 序号 (item), 名称 (name), 数量 (quantity), 型号 (part or value), 标号
(references), 封装 (footprint). Some rows name the supplier listing a part was bought from.

### Schematic sheets

| Sheet | Content |
|-------|---------|
| (top) | How the sheets connect |
| ESP8266_MCU | ESP8266 module (GBS-Control) and its CH340T USB serial chip |
| TV5725 | The TV5725 scaler |
| SDRAM | The scaler's frame memory |
| SI5351 | Si5351 clock generator |
| MS9288A | HDMI output |
| RGBS_IN | RGB SCART input, with the LM1881 sync separator and a TS5A23159 analog switch |
| VGA_IN, YPBPR_IN | VGA and Component inputs |
| AVSV2YPBPR | S-Video and Composite: ADV7280 decoder, ADV7391 encoder and the HC32F460 that drives them (the ADV Controller) |
| STV9426 | TV menu (OSD) chip, IR receiver, TS5V330 video switch |
| AUDIO | PT2257 volume control |
| POWER | Power supply |

### Datasheets

| File | Chip |
|------|------|
| `2013041613555642.pdf` | TrueView 5700 data sheet (the TV5725 scaler family) |
| `tv5725_registers_definition_v1.1.pdf` | TV5725 register definitions |
| `ADV7280.pdf` | ADV7280 video decoder |
| `ADV7280_7281_7282_7283_UG-637.pdf` | ADV7280 hardware reference manual (UG-637) |
| `adv7390-7391-7392-7393.pdf` | ADV7391 video encoder |
| `HC32F460 series Datasheet Rev1.3.pdf` | HC32F460 microcontroller (ADV Controller) |
| `STV9425-STV9425B.PDF` | STV9425/STV9425B OSD (the board carries an STV9426) |

## About these files

They are published for reference. The firmware supports the ready-made GBSC PRO from the
[official store](https://www.aliexpress.com/item/3256808268415575.html), where the ADV Controller's bootloader
(HCMGBoot, the vendor's USB bootloader in the first 64 KB of the HC32F460) comes installed; it is not part of this
repository. Flashing the firmware: [First install](../README.md#first-install-usb).

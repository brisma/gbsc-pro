#!/usr/bin/env python3
"""Build one channel of GBSC PRO (gzip image, no signature: the firmware is open) and lay out the
OTA files:
  <out>/v1/fw/gbsc-pro-<version>.bin.gz (+ .sha256 for the S3 object metadata)
  <out>/v1/<channel>/manifest.txt   (version, size, sha256, file, notes)
It never uploads: the GitHub Actions workflow copies <out>/v1 to the bucket."""
import argparse
import gzip
import hashlib
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "ota"))
import version as fwversion  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FW_DIR = os.path.join(REPO, "gbs-control")
BIN = os.path.join(os.environ.get("PLATFORMIO_BUILD_DIR") or os.path.join(FW_DIR, ".pio", "build"), "gbsc-pro",
                   "firmware.bin")   # the container keeps the PlatformIO build dir in a volume (compose.yaml)
VERSION_H = os.path.join(FW_DIR, "pro", "gbs-control-pro.h")
OTA_UPDATE_H = os.path.join(FW_DIR, "pro", "update", "ota-update.h")
FLASH_MODE_DIO = 0x02


def firmware_version():
    m = re.search(r'#define GBS_FW_VERSION "([^"]+)"', open(VERSION_H).read())
    return m.group(1)


def gzip_aligned(data):
    """gzip with a total length that is a multiple of 4 (the ESP8266 core Updater reads the flash in 32-bit
    words). Padding goes into the header's FNAME field (skipped by eboot's uzlib), so the image stays one valid
    gzip member."""
    gz = gzip.compress(data, compresslevel=9, mtime=0)
    pad = (-len(gz)) % 4
    if pad == 0:
        return gz
    if gz[3] & 0x08:
        sys.exit("unexpected gzip FNAME")
    name = b"g" * (pad - 1) + b"\0"           # FNAME: pad - 1 characters + NUL = pad bytes
    return gz[:3] + bytes([gz[3] | 0x08]) + gz[4:10] + name + gz[10:]


def make_image(raw):
    if raw[0] != 0xE9:
        sys.exit("firmware.bin is not an ESP8266 image")
    raw = bytearray(raw)
    raw[2] = FLASH_MODE_DIO                       # header byte 2: flash mode (not covered by the image checksum)
    return gzip_aligned(bytes(raw))


def raw_from_image(image):
    """The raw firmware back from a published image (flash mode byte already DIO): a re-run lays out its zip from
    the image the OTA server holds, not from a new build (kBuildId makes every build's bytes differ)."""
    raw = gzip.decompress(image)
    if not raw or raw[0] != 0xE9:
        sys.exit("the image does not hold an ESP8266 firmware")
    return raw


CHANNELS = {"stable": 0, "beta": 1}   # GBS_FW_CHANNEL (ota-plan.h)


def make_manifest(image, version, file, notes):
    """manifest.txt v1 within the device's limits (pro/update/update-manifest): version 1..15 of [0-9A-Za-z.-],
    file a safe relative path under 64 characters, notes under 96: else every device reports "manifest"."""
    if not fwversion.valid(version):
        sys.exit(f"version {version!r}: 1..15 characters of 0-9 A-Z a-z . - (device manifest limit)")
    if not re.fullmatch(r"[0-9A-Za-z._/\-]{1,63}", file) or ".." in file or file.startswith("/"):
        sys.exit(f"file {file!r}: a relative path of 0-9 A-Z a-z . _ / -, under 64 characters, no ..")
    if len(notes) >= 96 or "\n" in notes or "\r" in notes:
        sys.exit(f"notes: one line under 96 characters (device manifest limit), got {len(notes)}")
    text = (f"version={version}\n"
            f"size={len(image)}\n"
            f"sha256={hashlib.sha256(image).hexdigest()}\n"
            f"file={file}\n")
    if notes:
        text += f"notes={notes}\n"
    return text


def check_version(channel, version, base):
    """The full version must be base (Stable) or base-betaN (the channel compiled into the build)."""
    if channel not in CHANNELS:
        sys.exit(f"channel {channel!r}: stable or beta")
    if fwversion.channel_of(version) != channel or not fwversion.valid(version):
        sys.exit(f"version {version!r} is not a {channel} version (X.Y.Z, X.Y.Z-betaN, <= 15)")
    if version != base and not version.startswith(base + "-"):
        sys.exit(f"version {version!r} does not start with GBS_FW_VERSION {base}")


def check_marker(raw, version):
    """The built firmware must report exactly this version (GBSCVER=<version>; marker)."""
    m = re.search(rb"GBSCVER=([0-9A-Za-z.\-]+);", raw)
    if not m or m.group(1).decode() != version:
        sys.exit(f"firmware reports {m.group(1).decode() if m else 'no version'}, expected {version}")


def ota_base_default():
    """GBSC_OTA_BASE of the firmware (pro/update/ota-update.h): the base every published build checks."""
    return re.search(r'#define GBSC_OTA_BASE "([^"]+)"', open(OTA_UPDATE_H).read()).group(1)


def check_ota_base(base):
    """A base the device can use: plain HTTP (no TLS on the device), ending in /v1/, at most 64 characters (the image
    URL buffer is 128 with a file name under 64), no quote or space (it becomes a C string in the build flags)."""
    if not re.fullmatch(r"http://[0-9A-Za-z.\-]+(:[0-9]{1,5})?/([0-9A-Za-z._\-]+/)*v1/", base) or len(base) > 64:
        sys.exit(f"ota base {base!r}: http://host[:port]/.../v1/, at most 64 characters")


def check_firmware_base(raw, base):
    """The built firmware must hold exactly this base as a C string."""
    if base.encode() + b"\0" not in raw:
        sys.exit(f"the firmware does not check {base} (stale build or lost GBSC_OTA_BASE flag?)")


def build_flags(channel, version, ota_base=None):
    """PLATFORMIO_BUILD_FLAGS for a channel build: GBS_FW_CHANNEL, the version suffix and, for a local test server
    only, GBSC_OTA_BASE."""
    suffix = version[version.index("-"):] if "-" in version else ""
    flags = f'-DGBS_FW_CHANNEL={CHANNELS[channel]} -DGBS_FW_VERSION_SUFFIX=\\"{suffix}\\"'
    if ota_base and ota_base != ota_base_default():
        flags += f' -DGBSC_OTA_BASE=\\"{ota_base}\\"'
    return flags


def write_layout(out, channel, version, image, notes):
    """Writes v1/fw/<image> (refuses other bytes under an existing name), its .sha256 and v1/<channel>/manifest.txt."""
    rel = f"fw/gbsc-pro-{version}.bin.gz"
    img = os.path.join(out, "v1", *rel.split("/"))   # one separator kind on every system
    man = os.path.join(out, "v1", channel, "manifest.txt")
    text = make_manifest(image, version, rel, notes)
    if os.path.exists(img) and open(img, "rb").read() != image:
        sys.exit(f"{img} exists with other bytes: an image name is never reused")
    os.makedirs(os.path.dirname(img), exist_ok=True)
    os.makedirs(os.path.dirname(man), exist_ok=True)
    open(img, "wb").write(image)
    open(img + ".sha256", "w").write(hashlib.sha256(image).hexdigest() + "\n")
    open(man, "w", newline="\n").write(text)
    return img, man


ADV_CONTROLLER_BIN = os.path.join(REPO, "adv-controller", "release", "adv-controller.bin")
ADV_CONTROLLER_VERSION = os.path.join(REPO, "adv-controller", "VERSION")


def check_adv_controller(firmware, adv_controller):
    """The ESP image must carry the committed ADV Controller image verbatim (PROGMEM is stored raw) and its version must
    be adv-controller/VERSION. Returns the ADV Controller version."""
    sys.path.insert(0, os.path.join(FW_DIR, "tools"))
    import importlib.util
    spec = importlib.util.spec_from_file_location("adv_controller_image", os.path.join(FW_DIR, "tools", "adv-controller-image.py"))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    version, _crc = mod.parse_image(adv_controller)
    want = open(ADV_CONTROLLER_VERSION).read().strip()
    if version != want:
        sys.exit(f"ADV Controller image version {version} != adv-controller/VERSION {want}: run make release in adv-controller/")
    if adv_controller not in firmware:
        sys.exit("the ESP firmware does not embed adv-controller/release/adv-controller.bin (stale build?)")
    return version


README_FILES = [
    ("gbsc-pro-{version}.bin", ["GBS-Control for a USB flash (ESP8266)"]),
    ("gbsc-pro-{version}.bin.gz", ["GBS-Control for an update over WiFi (the flasher, the web app or the TV menu)"]),
    ("adv-controller-{adv}.bin", ["ADV Controller for a USB flash (HCMGBoot); over WiFi it updates from GBS-Control"]),
    ("GBSC PRO Flasher", ["macOS (.app), Windows (.exe) and Linux (.AppImage / .deb): it preselects the files",
                          "of this folder. Keep it in this folder."]),
    ("gbsc-flasher", ["The same flasher on the command line: gbsc-flasher-macos, gbsc-flasher.exe,",
                      "gbsc-flasher-linux. Run it with --help."]),
    ("esp8266-stub.json", ["esptool's ESP8266 flasher stub (GPL-2.0-or-later, LICENSES/), used by the flasher",
                           "for fast, checked USB writes; without it the flasher writes slowly and unchecked."]),
]

README_FOOTER = """
macOS: the app is not signed by Apple. Right-click it, choose Open, then Open again (once). The command line
flasher is blocked the same way: run xattr -d com.apple.quarantine gbsc-flasher-macos in this folder (once).
Windows: SmartScreen may warn. Choose "More info", then "Run anyway" (once). The first "Find on the network"
asks for the Windows Firewall permission: choose Allow, or the GBSC cannot answer the search.
"""


def readme_zip(version, adv):
    """README.txt of the release zip: the file column is as wide as the longest name (a -betaN or -devN version
    is longer than a plain one)."""
    names = [n.format(version=version, adv=adv) for n, _ in README_FILES]
    width = max(len(n) for n in names) + 3
    lines = [f"GBSC PRO {version}", "", "Files"]
    for name, (_, desc) in zip(names, README_FILES):
        lines.append(f"  {name.ljust(width)}{desc[0]}")
        lines += [" " * (2 + width) + d for d in desc[1:]]
    return "\n".join(lines) + "\n" + README_FOOTER


def write_release_zip(out, version, raw, image_gz, adv_controller, adv_controller_version, flasher_dir):
    """<out>/gbsc-pro-<version>.zip: one folder with the firmware files, the flasher binaries (flasher_dir as laid
    out by tools/flasher-build.py, stub and licences included) and README.txt; executable bits are kept."""
    import stat
    import zipfile
    top = f"gbsc-pro-{version}/"
    path = os.path.join(out, f"gbsc-pro-{version}.zip")

    def add_bytes(z, name, data, mode=0o644):
        info = zipfile.ZipInfo(top + name, date_time=(2026, 1, 1, 0, 0, 0))
        info.external_attr = (stat.S_IFREG | mode) << 16
        info.compress_type = zipfile.ZIP_DEFLATED
        z.writestr(info, data)

    with zipfile.ZipFile(path, "w") as z:
        add_bytes(z, f"gbsc-pro-{version}.bin", raw)
        add_bytes(z, f"gbsc-pro-{version}.bin.gz", image_gz)
        add_bytes(z, f"adv-controller-{adv_controller_version}.bin", adv_controller)
        add_bytes(z, "README.txt", readme_zip(version, adv_controller_version))
        for root, _dirs, files in os.walk(flasher_dir):
            for f in sorted(files):
                src = os.path.join(root, f)
                rel = os.path.relpath(src, flasher_dir).replace(os.sep, "/")
                add_bytes(z, rel, open(src, "rb").read(), os.stat(src).st_mode & 0o777)
    return path


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--channel", required=True, choices=sorted(CHANNELS))
    ap.add_argument("--version", required=True, help="full version: X.Y.Z or X.Y.Z-betaN")
    ap.add_argument("--skip-build", action="store_true")
    ap.add_argument("--from-image", default="", help="lay out from this published gbsc-pro-<version>.bin.gz instead of "
                    "building (a re-run of a release: the same bytes as the OTA server)")
    ap.add_argument("--out", default=os.path.join(REPO, "dist"))
    ap.add_argument("--notes", default="", help="release notes URL (Stable default: the GitHub tag page)")
    ap.add_argument("--flasher", default="", help="folder laid out by tools/flasher-build.py: also write the release "
                    "zip gbsc-pro-<version>.zip (firmware + flasher)")
    ap.add_argument("--ota-base", default="", help="local test server only: the base URL the "
                    "build checks instead of the published one; never publish such an image")
    args = ap.parse_args()

    base = args.ota_base or ota_base_default()
    check_ota_base(base)
    check_version(args.channel, args.version, firmware_version())
    published = open(args.from_image, "rb").read() if args.from_image else None
    if not args.skip_build and published is None:
        env = dict(os.environ, PLATFORMIO_BUILD_FLAGS=build_flags(args.channel, args.version, base))
        subprocess.run(["pio", "run", "-e", "gbsc-pro"], cwd=FW_DIR, check=True, env=env)
    raw = raw_from_image(published) if published is not None else open(BIN, "rb").read()
    check_marker(raw, args.version)
    check_firmware_base(raw, base)
    adv_controller = open(ADV_CONTROLLER_BIN, "rb").read()
    adv_controller_version = check_adv_controller(raw, adv_controller)
    image = published if published is not None else make_image(raw)
    notes = args.notes or (f"https://github.com/brisma/gbsc-pro/releases/tag/v{args.version}" if args.channel == "stable" else "")
    img, man = write_layout(args.out, args.channel, args.version, image, notes)
    adv_controller_path = os.path.join(args.out, f"gbsc-pro-adv-controller-{adv_controller_version}.bin")   # USB flasher asset (first v3 install)
    open(adv_controller_path, "wb").write(adv_controller)
    print(f"{img}\n{man}\n{adv_controller_path}\nversion={args.version}\nsize={len(image)}\nsha256={hashlib.sha256(image).hexdigest()}")
    if args.flasher:
        # the USB .bin from the image (flash mode DIO, as the OTA image): the same bytes on a first run and a re-run
        # (--from-image), and esptool's default --flash_mode keep writes DIO too
        print(write_release_zip(args.out, args.version, raw_from_image(image), image, adv_controller,
                                adv_controller_version, args.flasher))


if __name__ == "__main__":
    main()

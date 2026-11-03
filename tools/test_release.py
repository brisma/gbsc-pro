#!/usr/bin/env python3
"""Self-test of tools/release.py (no firmware build): python3 -I tools/test_release.py -v"""
import gzip
import hashlib
import shutil
import tempfile
import os
import struct
import subprocess
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import release  # noqa: E402


class ReleaseImageTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.raw = bytes([0xE9, 0x03, 0x00, 0x20]) + os.urandom(5000)   # fake qio image header + payload
        cls.image = release.make_image(cls.raw)

    # No signature: the image is one gzip member and nothing after it
    def test_image_is_a_plain_gzip(self):
        self.assertEqual(self.image[:2], b"\x1f\x8b")
        self.assertEqual(struct.unpack("<I", self.image[-4:])[0], len(self.raw))   # gzip ISIZE is the last field
        self.assertEqual(gzip.decompress(self.image)[3:], self.raw[3:])

    # Kept from the signed format: a length that is a multiple of 4 (the core Updater reads the flash in words)
    def test_gzip_is_a_multiple_of_4_for_any_input(self):
        for extra in range(8):
            raw = self.raw + bytes(extra)
            gz = release.make_image(raw)
            self.assertEqual(len(gz) % 4, 0)
            self.assertEqual(gzip.decompress(gz)[3:], raw[3:])

    def test_flash_mode_patched_to_dio(self):
        self.assertEqual(gzip.decompress(self.image)[2], 0x02)

    # A re-run of a published release lays out the zip from the published image (the build embeds its date and
    # time, so a new build has other bytes): the raw image comes back from the gzip, DIO header included
    def test_raw_from_a_published_image(self):
        raw = release.raw_from_image(self.image)
        self.assertEqual(raw[2], 0x02)
        self.assertEqual(raw[3:], self.raw[3:])
        self.assertEqual(release.make_image(raw), self.image)
        with self.assertRaises(SystemExit):
            release.raw_from_image(gzip.compress(b"<html>not firmware</html>"))

    def test_reproducible(self):
        self.assertEqual(release.make_image(self.raw), self.image)


class ManifestTest(unittest.TestCase):
    """OTA manifest v1: keys version, size, sha256, file, optional notes."""

    def setUp(self):
        self.image = release.make_image(bytes([0xE9, 0x03, 0x00, 0x20]) + os.urandom(1000))

    def test_manifest_v1_keys(self):
        text = release.make_manifest(self.image, "3.1.0-beta2", "fw/gbsc-pro-3.1.0-beta2.bin.gz", "")
        self.assertTrue(text.endswith("\n"))
        self.assertNotIn("\r", text)
        lines = dict(l.split("=", 1) for l in text.split("\n")[:-1])
        self.assertEqual(set(lines), {"version", "size", "sha256", "file"})
        self.assertEqual(lines["size"], str(len(self.image)))
        self.assertEqual(lines["sha256"], hashlib.sha256(self.image).hexdigest())
        self.assertEqual(lines["file"], "fw/gbsc-pro-3.1.0-beta2.bin.gz")
        text = release.make_manifest(self.image, "3.1.0", "fw/gbsc-pro-3.1.0.bin.gz", "https://x/n")
        self.assertIn("notes=https://x/n\n", text)

    def test_manifest_refuses_what_the_device_cannot_parse(self):
        release.make_manifest(self.image, "3.0.0", "fw/a.bin.gz", "n" * 95)
        for v, f, notes in (("3.0.0", "fw/a", "n" * 96), ("3.0.0_rc1", "fw/a", ""), ("", "fw/a", ""),
                            ("3.10.10-beta1000", "fw/a", ""), ("3.0.0", "../a", ""), ("3.0.0", "fw/a b", "")):
            with self.assertRaises(SystemExit, msg=f"{v!r} {f!r} {len(notes)}"):
                release.make_manifest(self.image, v, f, notes)


class ChannelTest(unittest.TestCase):
    def test_version_must_match_channel(self):
        release.check_version("stable", "3.1.0", "3.1.0")
        release.check_version("beta", "3.1.0-beta4", "3.1.0")
        for ch, v in (("stable", "3.1.0-beta4"), ("beta", "3.1.0"), ("beta", "3.1.0-dev1"), ("beta", "3.2.0-beta1"),
                      ("dev", "3.1.0-dev17"), ("nightly", "3.1.0")):   # no Dev channel
            with self.assertRaises(SystemExit, msg=f"{ch} {v}"):
                release.check_version(ch, v, "3.1.0")

    def test_marker_must_match(self):
        release.check_marker(b"..GBSCVER=3.1.0-beta4;..", "3.1.0-beta4")
        for raw in (b"..GBSCVER=3.1.0-dev0;..", b"no marker"):
            with self.assertRaises(SystemExit):
                release.check_marker(raw, "3.1.0-beta4")

    def test_build_flags(self):
        self.assertEqual(release.build_flags("beta", "3.1.0-beta4"),
                         '-DGBS_FW_CHANNEL=1 -DGBS_FW_VERSION_SUFFIX=\\"-beta4\\"')
        self.assertEqual(release.build_flags("stable", "3.1.0"), '-DGBS_FW_CHANNEL=0 -DGBS_FW_VERSION_SUFFIX=\\"\\"')

    # A local test server needs its own base; the published builds keep the default
    def test_build_flags_ota_base(self):
        self.assertEqual(release.build_flags("beta", "3.1.0-beta2", release.ota_base_default()),
                         '-DGBS_FW_CHANNEL=1 -DGBS_FW_VERSION_SUFFIX=\\"-beta2\\"')
        self.assertEqual(release.build_flags("beta", "3.1.0-beta2", "http://192.168.0.10:8000/v1/"),
                         '-DGBS_FW_CHANNEL=1 -DGBS_FW_VERSION_SUFFIX=\\"-beta2\\" '
                         '-DGBSC_OTA_BASE=\\"http://192.168.0.10:8000/v1/\\"')

    def test_ota_base_default_is_the_firmware_one(self):
        self.assertEqual(release.ota_base_default(), "http://gbsc.brisma.it/v1/")

    def test_ota_base_must_be_plain_http_ending_in_v1(self):
        release.check_ota_base("http://192.168.0.10:8000/v1/")
        for bad in ("https://gbsc.brisma.it/v1/", "http://192.168.0.10:8000/", "http://host/v1", "http://a\"b/v1/",
                    "http://" + "h" * 70 + "/v1/"):
            with self.assertRaises(SystemExit, msg=bad):
                release.check_ota_base(bad)

    # The built firmware must really hold the base it was asked for (a stale build or a lost flag would publish an
    # image that checks another server)
    def test_firmware_base_must_match(self):
        release.check_firmware_base(b"..http://gbsc.brisma.it/v1/\0..", "http://gbsc.brisma.it/v1/")
        for raw in (b"..http://192.168.0.10:8000/v1/\0..", b"..http://gbsc.brisma.it/v1/x\0..", b"nothing"):
            with self.assertRaises(SystemExit):
                release.check_firmware_base(raw, "http://gbsc.brisma.it/v1/")

    def test_channel_layout(self):
        import tempfile
        image = release.make_image(bytes([0xE9, 0x03, 0x00, 0x20]) + os.urandom(500))
        with tempfile.TemporaryDirectory() as out:
            img, man = release.write_layout(out, "beta", "3.1.0-beta4", image, "")
            self.assertEqual(img, os.path.join(out, "v1", "fw", "gbsc-pro-3.1.0-beta4.bin.gz"))
            self.assertEqual(man, os.path.join(out, "v1", "beta", "manifest.txt"))
            self.assertEqual(open(img, "rb").read(), image)
            self.assertEqual(open(img + ".sha256").read().strip(), hashlib.sha256(image).hexdigest())
            self.assertIn("file=fw/gbsc-pro-3.1.0-beta4.bin.gz\n", open(man).read())
            release.write_layout(out, "beta", "3.1.0-beta4", image, "")          # same bytes again: fine

    def test_refuses_overwrite_with_other_bytes(self):
        import tempfile
        a = release.make_image(bytes([0xE9, 0x03, 0x00, 0x20]) + b"a" * 500)
        b = release.make_image(bytes([0xE9, 0x03, 0x00, 0x20]) + b"b" * 500)
        with tempfile.TemporaryDirectory() as out:
            release.write_layout(out, "beta", "3.1.0-beta3", a, "")
            with self.assertRaises(SystemExit):
                release.write_layout(out, "beta", "3.1.0-beta3", b, "")


HARNESS = r"""
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "update-manifest.h"
int main(int argc, char** argv) {
    if (argc == 3 && !strcmp(argv[1], "manifest")) {           // parse a file, print the fields or FAIL
        static char buf[4096]; FILE* f = fopen(argv[2], "rb"); size_t n = fread(buf, 1, sizeof(buf), f); fclose(f);
        Manifest m;
        if (!manifestParse(buf, n, m)) { puts("FAIL"); return 0; }
        printf("%s|%u|", m.version, (unsigned)m.size);
        for (int i = 0; i < 32; i++) printf("%02x", m.sha256[i]);
        printf("|%s|%s\n", m.file, m.notes);
        return 0;
    }
    char a[64], b[64];                                         // "cmp": pairs on stdin, the sign per line
    while (scanf("%63s %63s", a, b) == 2) printf("%d\n", versionCompare(a, b));
    return 0;
}
"""


@unittest.skipUnless(shutil.which("c++"), "needs a host C++ compiler")
class DeviceParserTest(unittest.TestCase):
    """The firmware's own parser (pro/update/update-manifest.cpp) reads what release.py writes, and the device and
    tools/ota/version.py order versions the same way: both are frozen from 3.0.0."""

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp()
        src = os.path.join(cls.tmp, "h.cpp")
        open(src, "w").write(HARNESS)
        upd = os.path.join(release.FW_DIR, "pro", "update")
        cls.exe = os.path.join(cls.tmp, "h")
        subprocess.run(["c++", "-std=c++11", "-I", upd, src, os.path.join(upd, "update-manifest.cpp"), "-o", cls.exe],
                       check=True)

    def parse(self, text):
        path = os.path.join(self.tmp, "manifest.txt")
        open(path, "w", newline="\n").write(text)
        return subprocess.run([self.exe, "manifest", path], capture_output=True, text=True).stdout.strip()

    def test_device_parses_release_manifests(self):
        image = release.make_image(bytes([0xE9, 0x03, 0x00, 0x20]) + os.urandom(800))
        sha = hashlib.sha256(image).hexdigest()
        for v, notes in (("3.1.0", "https://github.com/brisma/gbsc-pro/releases/tag/v3.1.0"), ("3.1.0-beta12", ""),
                         ("10.10.10-dev99", "")):
            f = f"fw/gbsc-pro-{v}.bin.gz"
            self.assertEqual(self.parse(release.make_manifest(image, v, f, notes)), f"{v}|{len(image)}|{sha}|{f}|{notes}")

    def test_device_parses_rollback_manifests(self):
        # ota-publish.yml writes a rollback manifest with printf (no notes)
        text = "version=%s\nsize=%s\nsha256=%s\nfile=fw/gbsc-pro-%s.bin.gz\n" % ("3.0.1", 1234, "ab" * 32, "3.0.1")
        self.assertEqual(self.parse(text), "3.0.1|1234|" + "ab" * 32 + "|fw/gbsc-pro-3.0.1.bin.gz|")

    def test_version_order_matches_the_device(self):
        import random
        sys.path.insert(0, os.path.join(HERE, "ota"))
        import version
        rnd = random.Random(20261019)
        parts = ["0", "1", "2", "9", "10", "11", "99"]
        sufs = ["", "-dev0", "-dev9", "-dev10", "-beta1", "-beta12", "-rc1", "-local", "-beta.", "-dev.x", ".1", "a"]
        def one():
            return ".".join(rnd.choice(parts) for _ in range(rnd.choice([2, 3, 3, 3]))) + rnd.choice(sufs)
        pairs = [(one(), one()) for _ in range(3000)]
        out = subprocess.run([self.exe, "cmp"], input="".join(f"{a} {b}\n" for a, b in pairs), capture_output=True,
                             text=True).stdout.split()
        bad = [(a, b, int(c), version.compare(a, b)) for (a, b), c in zip(pairs, out) if int(c) != version.compare(a, b)]
        self.assertEqual(bad[:5], [])


class AdvControllerImageTest(unittest.TestCase):
    def test_adv_controller_image_script_self_test(self):
        r = subprocess.run([sys.executable, "-I", os.path.join(release.FW_DIR, "tools", "adv-controller-image.py"), "--self-test"],
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)

    def test_release_checks_the_embedded_adv_controller_image(self):
        adv_controller = open(release.ADV_CONTROLLER_BIN, "rb").read()
        self.assertTrue(release.check_adv_controller(b"head" + adv_controller + b"tail", adv_controller))
        with self.assertRaises(SystemExit):
            release.check_adv_controller(b"no image here", adv_controller)


class ReleaseZipTest(unittest.TestCase):
    """The release zip: firmware
    files next to the flasher, which preselects them"""

    def test_zip_holds_firmware_flasher_stub_and_licences(self):
        import zipfile
        d = tempfile.mkdtemp()
        try:
            fl = os.path.join(d, "flasher")
            os.makedirs(os.path.join(fl, "GBSC PRO Flasher.app", "Contents", "MacOS"))
            os.makedirs(os.path.join(fl, "LICENSES"))
            exe = os.path.join(fl, "GBSC PRO Flasher.app", "Contents", "MacOS", "gbsc-pro-flasher")
            open(exe, "wb").write(b"bin")
            os.chmod(exe, 0o755)
            open(os.path.join(fl, "gbsc-pro-flasher.exe"), "wb").write(b"exe")
            open(os.path.join(fl, "esp8266-stub.json"), "w").write("{}")
            open(os.path.join(fl, "LICENSES", "esptool-stub-GPL-2.0.txt"), "w").write("GPL")
            adv_version = open(release.ADV_CONTROLLER_VERSION).read().strip()
            path = release.write_release_zip(d, "3.0.1", b"RAW", b"GZ", b"ADV", adv_version, fl)
            self.assertEqual(os.path.basename(path), "gbsc-pro-3.0.1.zip")
            top = "gbsc-pro-3.0.1/"
            with zipfile.ZipFile(path) as z:   # closed before rmtree: Windows cannot delete an open file
                names = sorted(z.namelist())
                body = z.read(top + "gbsc-pro-3.0.1.bin")
                mode = z.getinfo(top + "GBSC PRO Flasher.app/Contents/MacOS/gbsc-pro-flasher").external_attr >> 16
            self.assertEqual(names, sorted([
                top + "gbsc-pro-3.0.1.bin", top + "gbsc-pro-3.0.1.bin.gz", top + f"adv-controller-{adv_version}.bin",
                top + "GBSC PRO Flasher.app/Contents/MacOS/gbsc-pro-flasher", top + "gbsc-pro-flasher.exe",
                top + "esp8266-stub.json", top + "LICENSES/esptool-stub-GPL-2.0.txt", top + "README.txt"]))
            self.assertEqual(body, b"RAW")
            if os.name != "nt":   # Windows has no executable bit to keep (releases are built on Linux)
                self.assertTrue(mode & 0o111, "the executable bit survives")
        finally:
            shutil.rmtree(d)

    def test_readme_columns_line_up_with_a_long_version(self):
        text = release.readme_zip("3.0.0-dev1", "3.0.0")
        rows = [l for l in text.splitlines() if l.startswith("  ") and not l.startswith("   ")]
        self.assertEqual(len(rows), 6, rows)
        # the description starts at the same column on every row, and on the wrapped lines below them
        starts = {len(l) - len(l[2:].split("  ", 1)[1].lstrip()) for l in rows}
        self.assertEqual(len(starts), 1, rows)
        col = starts.pop()
        wrapped = [l for l in text.splitlines() if l.startswith("   ")]
        self.assertTrue(wrapped)
        for l in wrapped:
            self.assertEqual(len(l) - len(l.lstrip()), col, l)

    def test_readme_explains_the_command_line_flasher(self):
        text = release.readme_zip("3.0.0", "3.0.0")
        for name in ("gbsc-flasher-macos", "gbsc-flasher.exe", "gbsc-flasher-linux"):
            self.assertIn(name, text)
        # Gatekeeper blocks the unsigned macOS CLI too; right-click Open does not exist for it
        self.assertIn("xattr -d com.apple.quarantine gbsc-flasher-macos", text)


if __name__ == "__main__":
    unittest.main()

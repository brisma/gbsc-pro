#!/usr/bin/env python3
"""Self-test of tools/release_assets.py: python3 -I tools/test_release_assets.py -v"""
import json
import os
import shutil
import stat
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import release_assets as ra  # noqa: E402


def write(path, data, mode=0o644):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)
    os.chmod(path, mode)


def flasher_dir(root):
    """A folder as tools/flasher-build.py lays it out for the three systems."""
    write(os.path.join(root, "esp8266-stub.json"), b"{}")
    write(os.path.join(root, "LICENSES", "GPL-3.0.txt"), b"gpl")
    write(os.path.join(root, "GBSC PRO Flasher.app", "Contents", "MacOS", "gbsc-pro-flasher"), b"mac app", 0o755)
    write(os.path.join(root, "GBSC PRO Flasher.app", "Contents", "Info.plist"), b"<plist/>")
    write(os.path.join(root, "gbsc-flasher-macos"), b"mac cli", 0o755)
    write(os.path.join(root, "GBSC PRO Flasher.exe"), b"win app", 0o755)
    write(os.path.join(root, "gbsc-flasher.exe"), b"win cli", 0o755)
    write(os.path.join(root, "gbsc-pro-flasher.AppImage"), b"appimage", 0o755)
    write(os.path.join(root, "gbsc-pro-flasher.deb"), b"deb")
    write(os.path.join(root, "gbsc-flasher-linux"), b"linux cli", 0o755)


class FingerprintTest(unittest.TestCase):
    def setUp(self):
        self.repo = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.repo)
        write(os.path.join(self.repo, "gbsc-pro-flasher", "core", "src", "lib.rs"), b"fn a() {}")
        write(os.path.join(self.repo, "gbsc-pro-flasher", "ui", "dist", "index.html"), b"<html>")
        write(os.path.join(self.repo, "gbsc-pro-flasher", "README.md"), b"notes")
        write(os.path.join(self.repo, "tools", "flasher-build.py"), b"build")
        self.files = ["gbsc-pro-flasher/core/src/lib.rs", "gbsc-pro-flasher/README.md", "tools/flasher-build.py"]

    def fp(self):
        return ra.flasher_fingerprint(self.repo, self.files)

    def test_same_inputs_same_fingerprint(self):
        self.assertEqual(self.fp(), self.fp())
        self.assertEqual(len(self.fp()), 64)

    def test_a_source_or_the_ui_bundle_changes_it(self):
        before = self.fp()
        write(os.path.join(self.repo, "gbsc-pro-flasher", "core", "src", "lib.rs"), b"fn b() {}")
        self.assertNotEqual(before, self.fp())
        before = self.fp()
        # a web app file the UI imports reaches the flasher through the built bundle
        write(os.path.join(self.repo, "gbsc-pro-flasher", "ui", "dist", "index.html"), b"<html lang=en>")
        self.assertNotEqual(before, self.fp())

    def test_docs_and_tests_do_not_change_it(self):
        before = self.fp()
        write(os.path.join(self.repo, "gbsc-pro-flasher", "README.md"), b"more notes")
        self.files.append("gbsc-pro-flasher/core/tests/esp_flash.rs")
        write(os.path.join(self.repo, "gbsc-pro-flasher", "core", "tests", "esp_flash.rs"), b"#[test]")
        self.files.append("gbsc-pro-flasher/ui/test/state.test.ts")
        write(os.path.join(self.repo, "gbsc-pro-flasher", "ui", "test", "state.test.ts"), b"test")
        self.assertEqual(before, self.fp())

    def test_without_the_ui_bundle_it_refuses(self):
        shutil.rmtree(os.path.join(self.repo, "gbsc-pro-flasher", "ui", "dist"))
        with self.assertRaises(SystemExit):
            self.fp()


class PickTest(unittest.TestCase):
    def test_newest_release_with_the_same_flashers(self):
        rel = [{"tag": "v3.1.0-beta5", "flasher": "bbb"}, {"tag": "v3.1.0-beta4", "flasher": "aaa"},
               {"tag": "v3.0.0", "flasher": "aaa"}]
        self.assertEqual(ra.pick_reuse(rel, "aaa"), "v3.1.0-beta4")
        self.assertIsNone(ra.pick_reuse(rel, "ccc"))
        self.assertIsNone(ra.pick_reuse([], "aaa"))
        with self.assertRaises(SystemExit):   # a failed fingerprint never matches a release that recorded "" too
            ra.pick_reuse([{"tag": "v3.0.0", "flasher": ""}], "")

    def test_reads_the_downloaded_manifests(self):
        d = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, d)
        for tag, fp in (("v3.0.0", "aaa"), ("v3.1.0-beta1", "bbb")):
            write(os.path.join(d, tag, "artifacts.json"), json.dumps({"version": tag[1:], "flasher": {"fingerprint": fp}}).encode())
        os.makedirs(os.path.join(d, "v2.4.1"))   # an old release without artifacts.json
        self.assertEqual(ra.pick_from_dir(d, ["v3.1.0-beta1", "v2.4.1", "v3.0.0"], "aaa"), "v3.0.0")


class PackTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.tmp)
        self.src = os.path.join(self.tmp, "flasher")
        flasher_dir(self.src)

    def test_one_zip_per_system_and_the_round_trip_keeps_modes(self):
        assets = os.path.join(self.tmp, "assets")
        paths = [ra.pack(self.src, assets, s) for s in ra.SYSTEMS]
        self.assertEqual(sorted(os.path.basename(p) for p in paths),
                         ["GBSC-PRO-Flasher-Linux.zip", "GBSC-PRO-Flasher-Windows.zip", "GBSC-PRO-Flasher-macOS.zip"])
        out = os.path.join(self.tmp, "back")
        ra.unpack(assets, out)
        for root, _dirs, files in os.walk(self.src):
            for f in files:
                a = os.path.join(root, f)
                b = os.path.join(out, os.path.relpath(a, self.src))
                self.assertEqual(open(a, "rb").read(), open(b, "rb").read(), b)
                self.assertEqual(stat.S_IMODE(os.stat(a).st_mode), stat.S_IMODE(os.stat(b).st_mode), b)

    def test_the_same_files_give_the_same_zip(self):
        a = open(ra.pack(self.src, os.path.join(self.tmp, "a"), "windows"), "rb").read()
        b = open(ra.pack(self.src, os.path.join(self.tmp, "b"), "windows"), "rb").read()
        self.assertEqual(a, b)

    def test_a_missing_binary_is_refused(self):
        os.remove(os.path.join(self.src, "gbsc-pro-flasher.AppImage"))
        with self.assertRaises(SystemExit):
            ra.pack(self.src, os.path.join(self.tmp, "assets"), "linux")


class ManifestTest(unittest.TestCase):
    def test_artifacts_json(self):
        m = ra.artifacts_manifest("3.0.0", "stable", "abc", "aaa", "built", "3.0.0", 0x51F83550)
        self.assertEqual(m["version"], "3.0.0")
        self.assertEqual(m["flasher"], {"fingerprint": "aaa", "from": "built"})
        self.assertEqual(m["adv_controller"], {"version": "3.0.0", "build": "51F83550"})
        self.assertEqual(m["commit"], "abc")


if __name__ == "__main__":
    unittest.main()

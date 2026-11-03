#!/usr/bin/env python3
"""Tests of tools/flasher-build.py (which Linux bundle file goes into the release folder).

Run: python tools/test_flasher_build.py -v
"""
import importlib.util
import os
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))


def load_flasher_build():
    spec = importlib.util.spec_from_file_location("flasher_build", os.path.join(HERE, "flasher-build.py"))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


class BundleFileTest(unittest.TestCase):
    def make(self, d, name, mtime):
        p = os.path.join(d, name)
        open(p, "w").close()
        os.utime(p, (mtime, mtime))
        return p

    def test_the_newest_build_wins_over_older_versions(self):
        fb = load_flasher_build()
        with tempfile.TemporaryDirectory() as root:
            d = os.path.join(root, "appimage")
            os.makedirs(d)
            self.make(d, "GBSC PRO Flasher_3.0.0_amd64.AppImage", 2000)
            new = self.make(d, "GBSC PRO Flasher_2.9.0_amd64.AppImage", 3000)   # names do not sort by age
            self.make(d, "GBSC PRO Flasher_3.1.0_amd64.AppImage.sig", 4000)
            self.assertEqual(fb.bundle_file(root, "appimage", ".AppImage"), new)

    def test_no_bundle_stops_the_build(self):
        fb = load_flasher_build()
        with tempfile.TemporaryDirectory() as root:
            os.makedirs(os.path.join(root, "deb"))
            with self.assertRaises(SystemExit):
                fb.bundle_file(root, "deb", ".deb")


if __name__ == "__main__":
    unittest.main()

#!/usr/bin/env python3
"""tools/ota/version.py against the firmware's frozen vectors (test_update_manifest test_version_order_channels)."""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import version  # noqa: E402

LOWER = [("3.0.0", "3.1.0"), ("3.0.9", "3.0.10"), ("3.1.0-dev9", "3.1.0-dev10"), ("3.1.0-dev50", "3.1.0-beta1"),
         ("3.1.0-beta7", "3.1.0"), ("3.1.0-local", "3.1.0-dev0"), ("3.1.0-a", "3.1.0-b"),
         ("3.0.0", "3.0.0.1"), ("3.0.0", "3.0.0a"), ("3.0", "3.0.1")]   # malformed: still one total order


class VersionTest(unittest.TestCase):
    def test_order_matches_the_device(self):
        for a, b in LOWER:
            self.assertEqual(version.compare(a, b), -1, (a, b))
            self.assertEqual(version.compare(b, a), 1, (b, a))
        self.assertEqual(version.compare("3.1.0-beta2", "3.1.0-beta2"), 0)

    def test_channel_of(self):
        self.assertEqual(version.channel_of("3.1.0"), "stable")
        self.assertEqual(version.channel_of("3.1.0-beta12"), "beta")
        self.assertIsNone(version.channel_of("3.1.0-dev0"))      # a local build: no channel publishes it
        self.assertIsNone(version.channel_of("3.1.0-rc1"))
        self.assertIsNone(version.channel_of("3.1"))

    def test_valid(self):
        self.assertTrue(version.valid("3.10.10-beta999"))       # 15 characters
        self.assertFalse(version.valid("3.10.10-beta1000"))     # 16
        self.assertFalse(version.valid("3.0.0_rc1"))
        self.assertFalse(version.valid(""))


if __name__ == "__main__":
    unittest.main()

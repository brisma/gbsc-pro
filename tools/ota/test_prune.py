#!/usr/bin/env python3
"""tools/ota/prune.py: which images the publishing job deletes from the bucket."""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import prune  # noqa: E402


def key(v):
    return f"v1/fw/gbsc-pro-{v}.bin.gz"


def manifest(v):
    return f"version={v}\nsize=1\nsha256={'0' * 64}\nfile=fw/gbsc-pro-{v}.bin.gz\n"


class PruneTest(unittest.TestCase):
    def test_keeps_the_newest_three_per_channel(self):
        keys = [key(f"3.1.0-beta{n}") for n in range(1, 8)] + [key("3.0.0"), key("3.0.1"), key("3.0.2"), key("3.0.3")]
        delete = prune.to_delete(keys, {}, keep=3)
        self.assertEqual(sorted(delete), sorted([key("3.1.0-beta1"), key("3.1.0-beta2"), key("3.1.0-beta3"),
                                                 key("3.1.0-beta4"), key("3.0.0")]))

    def test_order_is_numeric(self):
        keys = [key("3.1.0-beta9"), key("3.1.0-beta10"), key("3.1.0-beta11"), key("3.1.0-beta2")]
        self.assertEqual(prune.to_delete(keys, {}, keep=3), [key("3.1.0-beta2")])

    def test_keeps_what_a_live_manifest_names(self):
        keys = [key(f"3.1.0-beta{n}") for n in range(1, 6)]
        live = {"beta": manifest("3.1.0-beta1")}           # a rollback points at an old image
        delete = prune.to_delete(keys, live, keep=3)
        self.assertNotIn(key("3.1.0-beta1"), delete)
        self.assertEqual(delete, [key("3.1.0-beta2")])

    def test_never_touches_manifests_or_unknown_keys(self):
        keys = ["v1/stable/manifest.txt", "v1/beta/manifest.txt", "v1/fw/readme.txt", key("3.0.0")]
        self.assertEqual(prune.to_delete(keys, {}, keep=0), [key("3.0.0")])

    def test_listing_json(self):
        listing = {"Contents": [{"Key": key("3.0.0")}, {"Key": key("3.0.1")}]}
        self.assertEqual(prune.keys_from_listing(listing), [key("3.0.0"), key("3.0.1")])
        self.assertEqual(prune.keys_from_listing({}), [])


if __name__ == "__main__":
    unittest.main()

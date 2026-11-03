#!/usr/bin/env python3
"""Self-test of tools/regmap.py (register map for the web app): python3 -I tools/test_regmap.py -v"""
import gzip
import hashlib
import json
import os
import re
import sys
import tempfile
import time
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import regmap  # noqa: E402


def header_bytes(text):
    """The bytes of regmap_gz[] in a generated header."""
    body = text[text.index("{") + 1:text.index("}")]
    return bytes(int(x, 16) for x in re.findall(r"0x[0-9a-fA-F]{2}", body))


class RegmapTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.db = regmap.build(REPO)
        cls.tv = {row[0]: row for row in cls.db["tv5725"]}
        cls.chips = {c["id"]: c for c in cls.db["adv"]["chips"]}

    def reg(self, chip, map_id, addr):
        m = next(m for m in self.chips[chip]["maps"] if m["id"] == map_id)
        return next(r for r in m["regs"] if r[0] == addr)

    def test_tv5725_field_count_matches_header(self):
        with open(os.path.join(REPO, "gbs-control", "tv5725.h")) as f:
            count = len(re.findall(r"typedef\s+UReg<", f.read()))
        self.assertEqual(len(self.db["tv5725"]), count)
        self.assertEqual(count, 677)
        self.assertEqual(self.tv["STATUS_IF_HT_OK"][1:5], [0, 0, 1, 1])   # seg, reg, bit, width

    def test_segment0_low_registers_read_only(self):
        self.assertEqual(self.tv["STATUS_00"][5], "ro")
        for row in self.db["tv5725"]:
            if row[1] == 0 and row[2] < 0x40:
                self.assertEqual(row[5], "ro", row[0])
        self.assertEqual(self.tv["VDS_HSCALE"][5], "rw")

    def test_softreset_needs_confirm(self):
        self.assertEqual(self.tv["SFTRST_IF_RSTZ"][5], "confirm")
        self.assertEqual(self.tv["SFTRST_INT_RSTZ"][5], "confirm")
        self.assertEqual(self.tv["RESET_CONTROL_0x46"][5], "confirm")   # the whole register writes every reset bit

    def test_adv_chip_maps(self):
        maps = {m["id"]: m for m in self.chips["ADV7280"]["maps"]}
        self.assertEqual(maps["user_sub_map"]["mv"], "0x00")
        self.assertEqual(maps["user_sub_map_2"]["mv"], "0x40")
        self.assertEqual(maps["vpp_map"]["i2c"], "0x84")
        self.assertEqual([m["id"] for m in self.chips["ADV7391"]["maps"]], ["main"])
        self.assertEqual(self.reg("ADV7280", "user_sub_map", "0x00")[3], 0x09)   # presets/default.yaml
        self.assertEqual(len(self.db["adv"]["macros"]), 9)

    def test_dangerous_registers_flagged(self):
        self.assertEqual(self.reg("ADV7280", "user_sub_map", "0x0e")[6], "planner")
        for chip, map_id, addr in (("ADV7280", "user_sub_map", "0xfd"), ("ADV7280", "user_sub_map", "0x0f"),
                                   ("ADV7280", "vpp_map", "0x41"), ("ADV7391", "main", "0x17"),
                                   ("ADV7391", "main", "0x00")):
            self.assertEqual(self.reg(chip, map_id, addr)[6], "warn", (chip, addr))
        self.assertEqual(self.reg("ADV7280", "user_sub_map", "0x00")[6], "")
        kinds = {(a, m, r): k for a, m, r, k in self.db["adv"]["danger"]}
        for mp in (0x00, 0x20, 0x40):   # the map select register in every ADV7280 map
            self.assertEqual(kinds[(0x42, mp, 0x0E)], "planner")

    def test_managed_from_golden_traces(self):
        self.assertEqual(self.reg("ADV7280", "user_sub_map", "0x02")[7], 1)
        self.assertEqual(self.reg("ADV7280", "user_sub_map", "0x0a")[7], 1)
        self.assertEqual(self.reg("ADV7280", "user_sub_map", "0x01")[7], 0)   # never written by the ADV Controller
        managed = regmap.managed_registers(os.path.join(REPO, "adv-controller", "test", "golden"))
        self.assertIn((0x42, 0x00, 0x02), managed)
        self.assertNotIn((0x42, 0x80, 0x02), managed)   # map tracking: 0x9C writes sit in map 0x80
        self.assertIn((0x42, 0x80, 0x9C), managed)

    def test_header_is_gzip_of_json(self):
        with tempfile.TemporaryDirectory() as d:
            out, hdr = os.path.join(d, "regmap.json"), os.path.join(d, "regmap_gz.h")
            regmap.main(["--repo", REPO, "--out", out, "--header", hdr])
            with open(out, "rb") as f:
                js = f.read()
            with open(hdr) as f:
                text = f.read()
            self.assertIn("const uint8_t regmap_gz[] PROGMEM", text)
            data = header_bytes(text)
            self.assertEqual(gzip.decompress(data), js)
            self.assertIn("const unsigned int regmap_gz_len = %d;" % len(data), text)
            # ETag from the content: an incremental build after a YAML change gets a new one
            self.assertIn('const char regmap_etag[] = "%s";' % hashlib.sha256(data).hexdigest()[:16], text)
            self.assertEqual(json.loads(js)["version"], 1)

    def test_empty_header(self):
        with tempfile.TemporaryDirectory() as d:
            hdr = os.path.join(d, "regmap_gz.h")
            regmap.main(["--repo", REPO, "--header", hdr, "--empty"])
            with open(hdr) as f:
                text = f.read()
            self.assertIn("const unsigned int regmap_gz_len = 0;", text)
            self.assertIn('const char regmap_etag[] = "none";', text)
            self.assertIn("regmap_gz[]", text)

    def test_regmap_stale_detection(self):
        with tempfile.TemporaryDirectory() as d:
            src, out = os.path.join(d, "in.yaml"), os.path.join(d, "out.h")
            self.assertTrue(regmap.stale(out, [src]))            # no output yet
            open(src, "w").close()
            open(out, "w").close()
            past = time.time() - 100
            os.utime(src, (past, past))
            self.assertFalse(regmap.stale(out, [src]))
            os.utime(src, None)
            os.utime(out, (past, past))
            self.assertTrue(regmap.stale(out, [src]))            # an input newer than the output
        self.assertIn(os.path.join(REPO, "gbs-control", "tv5725.h"), regmap.inputs(REPO))


if __name__ == "__main__":
    unittest.main()

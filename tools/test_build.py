#!/usr/bin/env python3
"""Tests of gbs-control/tools/webui-build.py (stale page detection, the generated header).

Run: python tools/test_build.py -v
"""
import importlib.util
import os
import tempfile
import time
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)


def load_webui_build():
    spec = importlib.util.spec_from_file_location("webui_build", os.path.join(REPO, "gbs-control", "tools", "webui-build.py"))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


class WebuiStaleTest(unittest.TestCase):
    def test_webui_stale_detection(self):
        """The new web app: public/app, its build tools and gen/ feed webui_html.h; the old public/src does not."""
        wb = load_webui_build()
        with tempfile.TemporaryDirectory() as root:
            for d in ("public/app/pages", "public/tools", "public/gen", "public/src"):
                os.makedirs(os.path.join(root, d))
            src = os.path.join(root, "public", "app", "pages", "Quick.tsx")
            open(src, "w").write("x")
            self.assertTrue(wb.needs_build(root))          # no header yet
            hdr = os.path.join(root, "webui_html.h")
            open(hdr, "w").write("x")
            future = time.time() + 10
            os.utime(hdr, (future, future))
            self.assertFalse(wb.needs_build(root))         # header newer than every source
            for rel in ("public/app/pages/Quick.tsx", "public/tools/build-app.mjs", "public/gen/menu.json"):
                p = os.path.join(root, rel)
                open(p, "w").write("x")
                os.utime(p, (future + 10, future + 10))
                self.assertTrue(wb.needs_build(root), rel)  # a source changed after the header
                os.utime(p, (future - 5, future - 5))
            old = os.path.join(root, "public", "src", "index.ts")   # the old page is no source any more
            open(old, "w").write("x")
            os.utime(old, (future + 20, future + 20))
            self.assertFalse(wb.needs_build(root))

    def test_webui_header_from_the_page(self):
        """webui_html.h carries the gzipped page with the symbols gbs-control.ino uses."""
        import gzip
        import re
        wb = load_webui_build()
        with tempfile.TemporaryDirectory() as root:
            page = os.path.join(root, "app.html")
            open(page, "wb").write(b"<!doctype html><p>hello</p>")
            hdr = os.path.join(root, "webui_html.h")
            wb.write_header(page, hdr)
            text = open(hdr).read()
            self.assertIn("const uint8_t webui_html[] PROGMEM = {", text)
            data = bytes(int(x, 16) for x in re.findall(r"0x[0-9a-f]{2}", text.split("{", 1)[1].split("}", 1)[0]))
            self.assertEqual(gzip.decompress(data), b"<!doctype html><p>hello</p>")
            self.assertIn("const unsigned int webui_html_len = %d;" % len(data), text)


if __name__ == "__main__":
    unittest.main()

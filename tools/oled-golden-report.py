#!/usr/bin/env python3
"""Render the OLED golden screens (gbs-control/test/golden/oled/*.pbm, ASCII P1) as HTML for human review.

Usage: oled-golden-report.py <golden dir> <out dir>
Writes <out>/index.html with one SVG per golden (x4, lit pixels light blue on black, like the module).
"""
import html
import os
import sys

SCALE = 4
LIT = "#7fd4ff"


def parse(path):
    tokens = open(path, encoding="ascii").read().split()
    if tokens[0] != "P1":
        raise ValueError(f"{path}: not an ASCII PBM")
    w, h = int(tokens[1]), int(tokens[2])
    bits = "".join(tokens[3:])
    return w, h, bits


def svg(w, h, bits):
    out = [f'<svg width="{w * SCALE + 2}" height="{h * SCALE + 2}" xmlns="http://www.w3.org/2000/svg">',
           f'<rect x="0" y="0" width="{w * SCALE + 2}" height="{h * SCALE + 2}" fill="#000" stroke="#888"/>']
    for y in range(h):
        x = 0
        while x < w:
            if bits[y * w + x] == "1":
                run = x
                while run < w and bits[y * w + run] == "1":
                    run += 1
                out.append(f'<rect x="{1 + x * SCALE}" y="{1 + y * SCALE}" width="{(run - x) * SCALE}" height="{SCALE}" fill="{LIT}"/>')
                x = run
            else:
                x += 1
    out.append("</svg>")
    return "".join(out)


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    src, dst = sys.argv[1], sys.argv[2]
    os.makedirs(dst, exist_ok=True)
    parts = []
    for fn in sorted(os.listdir(src)):
        if not fn.endswith(".pbm") or ".actual." in fn:
            continue
        w, h, bits = parse(os.path.join(src, fn))
        parts.append(f"<section><h2>{html.escape(fn[:-4])}</h2>{svg(w, h, bits)}</section>")
    style = ("body{background:#20232b;color:#ddd;font:14px system-ui,sans-serif;margin:16px}"
             "section{display:inline-block;margin:0 24px 24px 0;vertical-align:top}h2{font-size:14px;margin:0 0 4px}")
    open(os.path.join(dst, "index.html"), "w", encoding="utf-8").write(
        f"<!doctype html><meta charset=utf-8><title>OLED goldens</title><style>{style}</style>" + "".join(parts))
    print(f"{len(parts)} screens -> {dst}")


if __name__ == "__main__":
    main()

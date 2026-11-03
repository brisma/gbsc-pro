#!/usr/bin/env python3
"""The GBSC PRO wordmark as SVG paths: Archivo is used only here, at build time.

python3 -I tools/logo-outline.py <archivo-900.woff2> <archivo-800.woff2>  -> JSON {"gbs","c","pro"} path data in the
logo's 232x44 viewBox: GBS right-aligned at x 86 (letter-spacing -1), C centred at 106, PRO from x 132 (800,
letter-spacing -0.5), baseline 36, size 40.
"""
import json
import sys

from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.ttLib import TTFont

SIZE, BASE = 40.0, 36.0


def run(font, text, spacing):
    cmap, gs, hmtx = font.getBestCmap(), font.getGlyphSet(), font["hmtx"]
    s = SIZE / font["head"].unitsPerEm
    glyphs = [cmap[ord(ch)] for ch in text]
    width = sum(hmtx[g][0] * s + spacing for g in glyphs)
    return glyphs, s, width


def outline(font, text, spacing, x0):
    glyphs, s, _ = run(font, text, spacing)
    gs, hmtx = font.getGlyphSet(), font["hmtx"]
    pen = SVGPathPen(gs, ntos=lambda v: ("%.1f" % v).rstrip("0").rstrip("."))
    x = x0
    for g in glyphs:
        gs[g].draw(TransformPen(pen, (s, 0, 0, -s, x, BASE)))
        x += hmtx[g][0] * s + spacing
    return pen.getCommands()


def main():
    black, bold = TTFont(sys.argv[1]), TTFont(sys.argv[2])
    _, _, w_gbs = run(black, "GBS", -1)
    _, _, w_c = run(black, "C", 0)
    print(json.dumps({
        "gbs": outline(black, "GBS", -1, 86 - w_gbs),
        "c": outline(black, "C", 0, 106 - w_c / 2),
        "pro": outline(bold, "PRO", -0.5, 132),
    }))


if __name__ == "__main__":
    main()

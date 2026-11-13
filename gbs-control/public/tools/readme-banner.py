#!/usr/bin/env python3
"""The banner on top of the repository's README: the wordmark of app/art/logo.tsx on a dark ground, with a line that
says what the project is and one with the inputs, outlined from Archivo so the picture needs no font.

python3 -I tools/readme-banner.py > ../../assets/gbsc-pro-banner.svg      (from gbs-control/public, after npm ci)
"""
import pathlib
import re
import sys

from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.ttLib import TTFont

SLOGAN = "Open-source firmware for the GBSC and GBSC PRO upscalers"
INPUTS, OUTPUT = "RGB SCART  ·  COMPONENT  ·  VGA  ·  S-VIDEO  ·  COMPOSITE", "HDMI"
W, H = 846, 226
BG, WHITE, YELLOW, INK, MUTED = "#0d0e11", "#ecebe8", "#f6d21a", "#1d1200", "#a8a59d"

root = pathlib.Path(__file__).resolve().parent.parent
fonts = {w: TTFont(root / f"node_modules/@fontsource/archivo/files/archivo-latin-{w}-normal.woff2") for w in (500, 600, 700)}
logo = (root / "app/art/logo.tsx").read_text()
GBS, C, PRO = (re.search(r'const %s = "([^"]+)"' % k, logo).group(1) for k in ("GBS", "C", "PRO"))


def text(s, x, y, size, weight, fill, anchor="start", spacing=0.0):
    """(path element, width) of s set at x, y (baseline); anchor "middle" centres it on x."""
    font = fonts[weight]
    glyphs, cmap, k = font.getGlyphSet(), font.getBestCmap(), size / font["head"].unitsPerEm
    run = [(cmap[ord(c)], glyphs[cmap[ord(c)]].width * k + spacing) for c in s]
    width = sum(a for _, a in run) - spacing
    x -= width / 2 if anchor == "middle" else 0
    d = []
    for glyph, advance in run:
        pen = SVGPathPen(glyphs, ntos=lambda v: ("%.1f" % v).rstrip("0").rstrip("."))
        glyphs[glyph].draw(TransformPen(pen, (k, 0, 0, -k, x, y)))
        d.append(pen.getCommands())
        x += advance
    return '<path d="%s" fill="%s"/>' % ("".join(d), fill), width


scale = 1.75                                    # the wordmark's box is 235 x 44
body = ('<g transform="translate(%.1f 44) scale(%s)"><path d="%s" fill="%s"/><rect x="87.5" y="2" width="37" height="40" rx="8" fill="%s"/>'
        '<path d="%s" fill="%s"/><path d="%s" fill="%s"/></g>' % ((W - 235 * scale) / 2 + 3 * scale, scale, GBS, WHITE, YELLOW, C, INK, PRO, YELLOW))
body += text(SLOGAN, W / 2, 158, 19, 500, WHITE, "middle")[0]
# the font's Latin subset has no arrow: it is drawn, between the inputs and the output
wl, wr, gap, arrow = text(INPUTS, 0, 0, 11.5, 600, MUTED, spacing=1.1)[1], text(OUTPUT, 0, 0, 11.5, 700, YELLOW, spacing=1.1)[1], 12, 20
x = (W - (wl + gap + arrow + gap + wr)) / 2
body += text(INPUTS, x, 190, 11.5, 600, MUTED, spacing=1.1)[0]
ax = x + wl + gap
body += ('<path d="M%.1f 186h%dM%.1f 182l4 4l-4 4" fill="none" stroke="%s" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"/>'
         % (ax, arrow - 1, ax + arrow - 5, YELLOW))
body += text(OUTPUT, ax + arrow + gap, 190, 11.5, 700, YELLOW, spacing=1.1)[0]
sys.stdout.write('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d %d" width="%d" height="%d" role="img" aria-label="GBSC PRO">'
                 '<rect width="%d" height="%d" rx="12" fill="%s"/>%s</svg>\n' % (W, H, W, H, W, H, BG, body))

#!/usr/bin/env python3
"""Render the OSD golden screens (gbs-control/test/golden/osd/*.txt) as HTML for human review.

Usage: osd-golden-report.py <golden dir> <out dir>
Writes one page per golden (<out>/<name>.html, sized to the screen) and <out>/index.html with all of them.
Spacing strips and character rows are drawn to scale from the header's CH and each strip's zoom.
"""
import html
import os
import re
import sys

PALETTE = ["#000000", "#0b22d6", "#17b52a", "#16d0e2", "#e21d1d", "#d21dd2", "#f1e51c", "#f2f2f2"]
SCALE = 0.5          # pixels per scan line
CELL_W = 18          # pixels per character cell
UDC_NAMES = "⓪①②③④⑤⑥⑦⑧⑨⑩⑪⑫⑬⑭⑮⑯⑰"
UDC_TITLES = ["speaker", "muted", "check", "lock", "wifi", "sync", "monitor", "slider full", "slider half", "slider track", "percent",
              "chevron left", "chevron right", "chevron up", "chevron down", "scrollbar track", "scrollbar thumb", "slider half right"]


def parse(path):
    lines = open(path, encoding="utf-8").read().splitlines()
    head = lines[0]
    m = re.search(r"CH=(\d+)", head)
    ch = int(m.group(1)) if m else 21
    legend = {}
    for ln in lines:
        if ln.startswith("attrs"):
            for tok in ln.split()[1:]:
                k, v = tok.split("=")
                legend[k] = int(v, 16)
    strips = []
    i = 1
    while i < len(lines):
        ln = lines[i]
        m = re.match(r"S (\d+)(?:x(\d+))?$", ln)
        if m:
            strips.append(("space", int(m.group(1)) * int(m.group(2) or 1)))
        m = re.match(r"R ([0-9A-F]{3}) ([z-])(?: DE=0)? \|(.*)\|", ln)
        if m:
            text = m.group(3)
            attrs = lines[i + 1].strip().strip("|") if i + 1 < len(lines) else ""
            strips.append(("row", m.group(2) == "z", text, attrs))
            i += 1
        i += 1
    return head, ch, legend, strips


def load_glyphs():
    """UDC bitmaps (12 x 18) from the driver golden, so UDC cells are drawn as the real glyphs."""
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "gbs-control", "test", "golden", "osd",
                        "udc-glyphs.txt")
    glyphs, cur = [], None
    if not os.path.exists(path):
        return glyphs
    for ln in open(path, encoding="utf-8").read().splitlines():
        if ln.startswith("UDC "):
            cur = []
            glyphs.append(cur)
        elif cur is not None and ln and set(ln) <= {".", "#"}:
            cur.append(ln)
    return glyphs


GLYPHS = load_glyphs()


def glyph_svg(rows, fg):
    rects = "".join(f'<rect x="{x}" y="{y}" width="1" height="1"/>'
                    for y, row in enumerate(rows) for x, c in enumerate(row) if c == "#")
    return (f'<svg viewBox="0 0 12 {len(rows)}" preserveAspectRatio="none" width="100%" height="100%" '
            f'fill="{fg}" shape-rendering="crispEdges">{rects}</svg>')


def cell_html(ch_text, attr):
    transparent = attr & 0x80
    fg = PALETTE[attr & 7]
    bg = "transparent" if transparent else PALETTE[(attr >> 4) & 7]
    title = ""
    glyph = html.escape(ch_text)
    if ch_text in UDC_NAMES:
        k = UDC_NAMES.index(ch_text)
        title = UDC_TITLES[k]
        if k < len(GLYPHS):
            glyph = glyph_svg(GLYPHS[k], fg)
    return f'<i style="color:{fg};background:{bg}" title="{title}">{glyph}</i>'


def screen_html(name, head, ch, legend, strips):
    out = [f'<section><h2>{html.escape(name)}</h2><p class="h">{html.escape(head)}</p><div class="tv">']
    total = 0
    for s in strips:
        if s[0] == "space":
            h = s[1] * SCALE
            total += s[1]
            out.append(f'<div class="sp" style="height:{h:.1f}px"></div>')
            continue
        _, zoom, text, attrs = s
        lines_h = ch * (2 if zoom else 1)
        total += lines_h
        cells = list(text)
        row = []
        for k, c in enumerate(cells):
            letter = attrs[k] if k < len(attrs) else "A"
            row.append(cell_html(c, legend.get(letter, 0x17)))
        out.append(f'<div class="row" style="height:{lines_h * SCALE:.1f}px;font-size:{lines_h * SCALE * 0.8:.1f}px">{"".join(row)}</div>')
    out.append("</div></section>")
    return "".join(out), total


STYLE = """
body{background:#20232b;color:#ddd;font:14px system-ui,sans-serif;margin:16px}
section{margin-bottom:28px}
h2{font-size:15px;margin:0 0 4px}
.h{color:#999;font:12px monospace;margin:0 0 6px}
.tv{display:inline-block;padding:8px;background:repeating-conic-gradient(#555 0 25%,#666 0 50%) 0 0/12px 12px}
.row{display:flex;line-height:1}
.row i{font-style:normal;width:CELLpx;display:flex;align-items:center;justify-content:center;font-family:monospace;overflow:hidden}
.sp{width:ROWpx}
""".replace("CELL", str(CELL_W)).replace("ROW", str(CELL_W * 30))


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    src, dst = sys.argv[1], sys.argv[2]
    os.makedirs(dst, exist_ok=True)
    index = []
    for fn in sorted(os.listdir(src)):
        if not fn.endswith(".txt") or fn.endswith(".actual.txt") or fn.startswith("udc"):
            continue
        name = fn[:-4]
        head, ch, legend, strips = parse(os.path.join(src, fn))
        body, _ = screen_html(name, head, ch, legend, strips)
        page = f"<!doctype html><meta charset=utf-8><title>{name}</title><style>{STYLE}</style>{body}"
        open(os.path.join(dst, name + ".html"), "w", encoding="utf-8").write(page)
        index.append(body)
    open(os.path.join(dst, "index.html"), "w", encoding="utf-8").write(
        f"<!doctype html><meta charset=utf-8><title>OSD goldens</title><style>{STYLE}</style>" + "".join(index))
    print(f"{len(index)} screens -> {dst}")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Material Symbols Rounded subset for the web app.

python3 -I tools/icons-subset.py <font.woff2> <out.json> <name>...
Pins wght 400, GRAD 0, opsz 24 (FILL kept: selected tabs use the filled form), keeps only the named icons by their
private-use codepoints (no ligature table: the page writes the codepoint), writes {"font": data URL, "map": {name:
codepoint}}. Exit 1 naming the icons the font does not have.
"""
import base64
import io
import json
import sys

from fontTools import subset
from fontTools.ttLib import TTFont
from fontTools.varLib import instancer


def ligature_names(font):
    """icon name -> glyph name, from the GSUB ligature lookups (letters -> icon glyph)."""
    cmap = font.getBestCmap()
    glyph_char = {}
    for cp, g in cmap.items():
        if cp < 0x80:
            glyph_char.setdefault(g, chr(cp))
    out = {}
    for lookup in font["GSUB"].table.LookupList.Lookup:
        for st in lookup.SubTable:
            if st.LookupType == 7:
                st = st.ExtSubTable
            if getattr(st, "LookupType", None) != 4:
                continue
            for first, ligs in st.ligatures.items():
                for lig in ligs:
                    seq = [first] + list(lig.Component)
                    if all(g in glyph_char for g in seq):
                        out.setdefault("".join(glyph_char[g] for g in seq).lower(), lig.LigGlyph)   # A and a share a glyph
    return out


def main():
    src, out_path, names = sys.argv[1], sys.argv[2], sorted(set(sys.argv[3:]))
    font = TTFont(src, lazy=False, recalcTimestamp=False)   # head.modified from the source: the same subset, the same bytes
    by_name = ligature_names(font)
    rev = {}
    for cp, g in font.getBestCmap().items():
        if cp >= 0xE000:
            rev.setdefault(g, cp)
    missing = [n for n in names if n not in by_name or by_name[n] not in rev]
    if missing:
        print("icons-subset: not in Material Symbols Rounded: " + ", ".join(missing))
        sys.exit(1)
    cps = {n: rev[by_name[n]] for n in names}
    opts = subset.Options()
    opts.layout_features = []
    opts.name_IDs = [1, 2]
    opts.notdef_outline = False
    opts.hinting = False
    sub = subset.Subsetter(opts)
    sub.populate(unicodes=list(cps.values()))
    sub.subset(font)                                                  # first the glyphs, then the axes
    font = instancer.instantiateVariableFont(font, {"wght": 400, "GRAD": 0, "opsz": 24})
    buf = io.BytesIO()
    font.flavor = "woff2"
    font.save(buf)
    data = "data:font/woff2;base64," + base64.b64encode(buf.getvalue()).decode()
    with open(out_path, "w") as f:
        json.dump({"font": data, "map": cps}, f, sort_keys=True)
    print("icons-subset: %d icons, %d B woff2" % (len(cps), len(buf.getvalue())))


if __name__ == "__main__":
    main()

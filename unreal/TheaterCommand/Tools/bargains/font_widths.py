#!/usr/bin/env python3
"""Writes font_widths.h: the kit fonts' advances (Content/Fonts) for the look test's and the preview's measure.

Usage: python3 font_widths.py <Content/Fonts folder> <out .h>
Each face: units per em, the line's height ((ascender - descender) / em, as Slate measures a line), its ascender (where
Slate sets the baseline under a line's top), the figures' height (the tallest of 0-9 over the baseline) and the capitals'
(OS/2), and the advance of ASCII 32-126 and the few other letters the look writes (the middle dot, the minus, the en
dash, the ellipsis).
"""
import sys
from fontTools.pens.boundsPen import BoundsPen
from fontTools.ttLib import TTFont

FACES = [("SerifRegular", "PTSerif-Regular.ttf"), ("SerifBold", "PTSerif-Bold.ttf"), ("SerifItalic", "PTSerif-Italic.ttf"),
         ("CourierRegular", "CourierPrime-Regular.ttf"), ("CourierBold", "CourierPrime-Bold.ttf")]
EXTRA = [0x00B7, 0x2212, 0x2013, 0x2026]

def main():
    folder, out = sys.argv[1], sys.argv[2]
    lines = ["#pragma once", "// Written by font_widths.py from Content/Fonts: the kit fonts' advances, in em. TEST AND PREVIEW ONLY.", "",
             "namespace FontWidths", "{", "\tstruct FFaceTable { double LineH; double Ascent; double FigureH; double CapH; double Ascii[95]; double Extra[4]; };",
             "\tinline constexpr unsigned ExtraCodes[4] = { 0x00B7, 0x2212, 0x2013, 0x2026 };"]
    for name, file in FACES:
        f = TTFont(f"{folder}/{file}")
        em = f["head"].unitsPerEm
        hhea = f["hhea"]
        cmap = f.getBestCmap()
        hmtx = f["hmtx"]
        def adv(code):
            g = cmap.get(code)
            return hmtx[g][0] / em if g else 0.5
        glyphs = f.getGlyphSet()
        def top(code):
            pen = BoundsPen(glyphs)
            glyphs[cmap[code]].draw(pen)
            return pen.bounds[3] / em if pen.bounds else 0.0
        figure = max(top(c) for c in range(ord("0"), ord("9") + 1))
        cap = f["OS/2"].sCapHeight / em
        ascii_ = ", ".join(f"{adv(c):.5f}" for c in range(32, 127))
        extra = ", ".join(f"{adv(c):.5f}" for c in EXTRA)
        lines.append(f"\tinline constexpr FFaceTable {name}{{ {(hhea.ascent - hhea.descent) / em:.5f}, {hhea.ascent / em:.5f}, {figure:.5f}, {cap:.5f}, {{ {ascii_} }}, {{ {extra} }} }};")
    lines += ["}", ""]
    open(out, "w").write("\n".join(lines))

main()

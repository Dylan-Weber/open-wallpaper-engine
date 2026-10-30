#!/usr/bin/env python3
"""Writes the kerning fonts for the text layout tests: KerningGpos.ttf kerns in its GPOS table, KerningKern.ttf in
its legacy 'kern' table.

    python3 make-kerning.py    (needs fontTools: pip install fonttools)

1000 units an em, ascender 800, descender 200, no line gap. Every glyph is a 400-unit square drawn 100..500 in a
600-unit advance. The pairs:

    pair   GPOS            kern table
    A V    -100 (glyphs)   -100
    T o    -150 (classes)  -150
    o T    -              -
"""
import os

from fontTools.feaLib.builder import addOpenTypeFeaturesFromString
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib import newTable
from fontTools.ttLib.tables._k_e_r_n import KernTable_format_0

LETTERS = 'AVTo'


def square():
    pen = TTGlyphPen(None)
    pen.moveTo((100, 0))
    pen.lineTo((100, 400))
    pen.lineTo((500, 400))
    pen.lineTo((500, 0))
    pen.closePath()
    return pen.glyph()


def build(path, gpos):
    order = ['.notdef', 'space'] + list(LETTERS)
    builder = FontBuilder(1000, isTTF=True)
    builder.setupGlyphOrder(order)
    builder.setupCharacterMap({0x20: 'space', **{ord(c): c for c in LETTERS}})
    builder.setupGlyf({name: square() for name in order})
    builder.setupHorizontalMetrics({name: (600, 100) for name in order})
    builder.setupHorizontalHeader(ascent=800, descent=-200, lineGap=0)
    builder.setupNameTable({'familyName': os.path.basename(path)[:-4], 'styleName': 'Regular'})
    builder.setupOS2(sTypoAscender=800, sTypoDescender=-200, sTypoLineGap=0, usWinAscent=800, usWinDescent=200)
    builder.setupPost()
    if gpos:
        addOpenTypeFeaturesFromString(builder.font, """
            languagesystem DFLT dflt;
            @round = [o];
            feature kern {
                pos A V -100;
                pos [T] @round -150;
            } kern;
        """)
    else:
        kern = newTable('kern')
        kern.version = 0
        table = KernTable_format_0()
        table.version, table.coverage, table.format = 0, 1, 0
        table.kernTable = {('A', 'V'): -100, ('T', 'o'): -150}
        kern.kernTables = [table]
        builder.font['kern'] = kern
    builder.save(path)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    build(os.path.join(here, 'KerningGpos.ttf'), True)
    build(os.path.join(here, 'KerningKern.ttf'), False)


if __name__ == '__main__':
    main()

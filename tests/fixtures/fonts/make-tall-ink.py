#!/usr/bin/env python3
"""Writes TallInk.ttf, a font whose letter reaches above its ascent, for the text background tests.

    python3 make-tall-ink.py    (needs fontTools: pip install fonttools)

1000 units an em, hhea ascent 500 and descent 500, no line gap; H (advance 700) is a rectangle 100..600 x 0..700,
200 units above the ascent.
"""
import os

from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen


def rectangle(box):
    pen = TTGlyphPen(None)
    if box:
        x0, y0, x1, y1 = box
        pen.moveTo((x0, y0))
        pen.lineTo((x0, y1))
        pen.lineTo((x1, y1))
        pen.lineTo((x1, y0))
        pen.closePath()
    return pen.glyph()


def main():
    builder = FontBuilder(1000, isTTF=True)
    builder.setupGlyphOrder(['.notdef', 'space', 'H'])
    builder.setupCharacterMap({0x20: 'space', ord('H'): 'H'})
    builder.setupGlyf({'.notdef': rectangle((100, 0, 600, 700)), 'space': rectangle(None),
                       'H': rectangle((100, 0, 600, 700))})
    builder.setupHorizontalMetrics({'.notdef': (700, 100), 'space': (300, 0), 'H': (700, 100)})
    builder.setupHorizontalHeader(ascent=500, descent=-500, lineGap=0)
    builder.setupNameTable({'familyName': 'TallInk', 'styleName': 'Regular'})
    builder.setupOS2(sTypoAscender=500, sTypoDescender=-500, sTypoLineGap=0, usWinAscent=500, usWinDescent=500)
    builder.setupPost()
    builder.save(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'TallInk.ttf'))


if __name__ == '__main__':
    main()

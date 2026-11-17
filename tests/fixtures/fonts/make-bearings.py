#!/usr/bin/env python3
"""Writes Bearings.ttf, a font of rectangles whose side bearings are known, for the text layout tests.

    python3 make-bearings.py    (needs fontTools: pip install fonttools)

1000 units an em, ascender 800, descender 200, no line gap. Each glyph is one rectangle:

    glyph  advance  ink x       ink y
    L      600      100..500    0..700    bare on both sides
    o      500      50..450     0..500
    j      300      -100..200   -200..700 hangs 100 left of the pen
    f      400      50..450     0..700    reaches 50 past its advance
    space  250      (none)
"""
import os

from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen

GLYPHS = {  # name: (character, advance, (x0, y0, x1, y1))
    'L': ('L', 600, (100, 0, 500, 700)),
    'o': ('o', 500, (50, 0, 450, 500)),
    'j': ('j', 300, (-100, -200, 200, 700)),
    'f': ('f', 400, (50, 0, 450, 700)),
}


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
    order = ['.notdef', 'space'] + list(GLYPHS)
    builder = FontBuilder(1000, isTTF=True)
    builder.setupGlyphOrder(order)
    builder.setupCharacterMap({0x20: 'space', **{ord(c): name for name, (c, _, _) in GLYPHS.items()}})
    outlines = {'.notdef': rectangle((50, 0, 450, 700)), 'space': rectangle(None)}
    metrics = {'.notdef': (500, 50), 'space': (250, 0)}
    for name, (_, advance, box) in GLYPHS.items():
        outlines[name] = rectangle(box)
        metrics[name] = (advance, box[0])
    builder.setupGlyf(outlines)
    builder.setupHorizontalMetrics(metrics)
    builder.setupHorizontalHeader(ascent=800, descent=-200, lineGap=0)
    builder.setupNameTable({'familyName': 'Bearings', 'styleName': 'Regular'})
    builder.setupOS2(sTypoAscender=800, sTypoDescender=-200, sTypoLineGap=0, usWinAscent=800, usWinDescent=200)
    builder.setupPost()
    builder.save(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'Bearings.ttf'))


if __name__ == '__main__':
    main()

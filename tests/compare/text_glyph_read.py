#!/usr/bin/env python3
"""Reads text-glyph-position pictures (make/text-glyph-position.py): for each row (one point size), the left and right edges
of its first stem (the I) and its top and bottom, in picture pixels, from the ink's coverage (white on black). Usage:
text_glyph_read.py PICTURE [PICTURE ...]"""
import sys

import numpy as np
from PIL import Image


def rows(picture):
    a = np.asarray(Image.open(picture).convert('L'), dtype=float) / 255
    a[:, :40] = 0   # the probe's strips
    ink_rows = a.max(axis=1) > 0.5
    found, start = [], None
    for y, on in enumerate(list(ink_rows) + [False]):
        if on and start is None:
            start = y
        elif not on and start is not None:
            found.append((start, y))
            start = None
    out = []
    for top, bottom in found:
        band = a[top:bottom]
        columns = band.max(axis=0)
        x0 = int(np.argmax(columns > 0.5))
        x1 = x0
        while x1 < len(columns) and columns[x1] > 0.02:
            x1 += 1
        lead = x0
        while lead > 0 and columns[lead - 1] > 0.02:
            lead -= 1
        stem = band[:, lead:x1].max(axis=0)
        # edges from coverage: the left edge is where the summed coverage starts
        left = lead + (1 - stem[0]) if len(stem) else None
        width = stem.sum()
        out.append((top, bottom, round(left, 2), round(left + width, 2), round(width, 2)))
    return out


if __name__ == '__main__':
    for path in sys.argv[1:]:
        print(path)
        for row in rows(path):
            print('  rows %4d-%4d  stem left %7.2f right %7.2f width %5.2f' % row)

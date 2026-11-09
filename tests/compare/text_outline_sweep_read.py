#!/usr/bin/env python3
"""Reads text-outline-sweep pictures (make/text-outline-sweep.py): for each H (outline thickness 0, 1, 2, 4, 8, 16), its white
letter's left and right edges, top and bottom (picture pixels, from where coverage passes one half), measured from the layer's
origin. Usage: text_outline_sweep_read.py PICTURE"""
import sys

import numpy as np
from PIL import Image

THICKNESSES = (0, 1, 2, 4, 8, 16)


def edges(profile):
    on = profile > 0.5
    xs = np.where(on)[0]
    if not len(xs): return None
    # sub-pixel: interpolate the half-coverage crossing at each end
    l, r = xs[0], xs[-1]
    left = l - (profile[l] - 0.5) / max(profile[l] - (profile[l - 1] if l else 0), 1e-6) + 0.5
    right = r + (profile[r] - 0.5) / max(profile[r] - (profile[r + 1] if r + 1 < len(profile) else 0), 1e-6) + 0.5
    return left, right


def letters(path):
    a = np.asarray(Image.open(path).convert('RGB')).astype(float) / 255
    s = a.shape[1] / 1920
    white = np.clip(np.minimum(a[..., 1], a[..., 2]), 0, 1)   # red outline: g, b ~0
    out = {}
    yc = int((1080 - 540) * s)
    for col, t in enumerate(THICKNESSES):
        x0 = int((100 + col * 300 - 40) * s); x1 = int((100 + col * 300 + 290) * s)
        band = white[int(yc - 250 * s):int(yc + 250 * s), x0:x1]
        cols = band.max(0); rows = band.max(1)
        lr = edges(cols); tb = edges(rows)
        origin = (100 + col * 300) * s
        out[t] = None if lr is None else (x0 + lr[0] - origin, x0 + lr[1] - origin, int(yc - 250 * s) + tb[0], int(yc - 250 * s) + tb[1])
    return out


if __name__ == '__main__':
    for t, box in letters(sys.argv[1]).items():
        print('t%-2d' % t, None if box is None else 'left %.2f right %.2f top %.2f bottom %.2f width %.2f' % (*box, box[1] - box[0]))

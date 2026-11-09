#!/usr/bin/env python3
"""Reads text-outline-offset pictures (make/text-outline-offset.py): the white letter's box (sub-pixel, from where coverage
crosses one half) of the plain letter (layer origin x 200) and of the outlined one (origin x 1100), each measured from its
layer's origin, in scene units, and the outlined letter's offset from the plain one. Usage: text_outline_offset_read.py PICTURE"""
import sys

import numpy as np
from PIL import Image


def crossing(profile):
    on = np.where(profile > 0.5)[0]
    if not len(on): return None
    l, r = on[0], on[-1]
    def frac(i, j):   # where coverage crosses 0.5 between pixel i (inside) and j (outside)
        a, b = profile[i], profile[j] if 0 <= j < len(profile) else 0.0
        return (a - 0.5) / max(a - b, 1e-6)
    return l + 0.5 - frac(l, l - 1), r + 0.5 + frac(r, r + 1)


def box(white, x0, x1, s):
    band = white[:, x0:x1]
    lr = crossing(band.max(0)); tb = crossing(band.max(1))
    return None if lr is None else ((x0 + lr[0]) / s, (x0 + lr[1]) / s, tb[0] / s, tb[1] / s)


def read(path):
    a = np.asarray(Image.open(path).convert('RGB')).astype(float) / 255
    s = a.shape[1] / 1920
    white = np.minimum(a[..., 1], a[..., 2])
    white[: int(130 * s)] = 0; white[int(860 * s):] = 0   # the probe's strips and records
    plain = box(white, int(150 * s), int(1050 * s), s)
    outlined = box(white, int(1050 * s), a.shape[1], s)
    plain = (plain[0] - 200, plain[1] - 200, plain[2], plain[3])
    outlined = (outlined[0] - 1100, outlined[1] - 1100, outlined[2], outlined[3])
    return plain, outlined


if __name__ == '__main__':
    plain, outlined = read(sys.argv[1])
    d = [o - p for o, p in zip(outlined, plain)]
    print('plain    left %.2f right %.2f top %.2f bottom %.2f' % plain)
    print('outlined left %.2f right %.2f top %.2f bottom %.2f' % outlined)
    print('offset   left %+.2f right %+.2f top %+.2f bottom %+.2f (scene units)' % tuple(d))

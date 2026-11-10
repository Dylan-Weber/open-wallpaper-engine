#!/usr/bin/env python3
"""Reads sprite-created-coins pictures: the probe's clock (g_Time) and the frame (0-3, a colour) of each layer on the row
y = 540 of the scene, in order of x. Usage: sprite_created_coins_read.py PICTURE [PICTURE ...]"""
import importlib.util
import os
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location('clock_probe', os.path.join(HERE, 'clock-probe.py'))
clock_probe = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(clock_probe)
COLORS = [(230, 40, 40), (40, 200, 60), (50, 70, 230), (240, 220, 30)]
LAYER_HALF = 120  # layers are 8 x 8 scaled by 30


def frame_of(pixel):
    distance = [sum(abs(a - b) for a, b in zip(color, pixel)) for color in COLORS]
    best = min(range(len(COLORS)), key=distance.__getitem__)
    return (best, distance[best]) if distance[best] < 60 else (None, distance[best])


def layers(picture):
    """(x, frame) of each run of one colour along the row, in scene units."""
    w, h = picture.size
    s = w / 1920
    y = int(540 * s)
    found, run = [], None
    for px in range(w):
        frame, _ = frame_of(picture.getpixel((px, y))[:3])
        if frame is not None and (run is None or run[0] == frame):
            run = (frame, run[1] if run else px, px)
        else:
            if run is not None and run[2] - run[1] > 60 * s:
                found.append((int((run[1] + run[2]) / 2 / s), run[0]))
            run = (frame, px, px) if frame is not None else None
    if run is not None and run[2] - run[1] > 60 * s:
        found.append((int((run[1] + run[2]) / 2 / s), run[0]))
    return found


if __name__ == '__main__':
    for path in sys.argv[1:]:
        picture = Image.open(path).convert('RGB')
        clock = clock_probe.read(picture)
        print('%s: g_Time frame %s (%s s)' % (path, clock, '%.3f' % (clock / 30) if clock is not None else '?'))
        for x, frame in layers(picture):
            print('   layer at x %4d: frame %d' % (x, frame))

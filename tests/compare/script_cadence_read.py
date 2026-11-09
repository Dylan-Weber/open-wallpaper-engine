#!/usr/bin/env python3
"""Reads a script-cadence picture (make/script-cadence.py): the probe's g_Time and scripts' frames, and each row's count
(A, B, C, D: update() runs of a visible, an invisible, an offscreen and a puppet layer, modulo 1000; F: engine.frametime in
0.1 ms; S: the refresh-synced sprite sheet's frame, one a rendered update).

    script_cadence_read.py PICTURE [PICTURE ...]
"""
import importlib.util
import os
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location('clock_probe', os.path.join(HERE, 'clock-probe.py'))
clock_probe = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(clock_probe)

# scene x of each row (1920 x 1080 scene), at y 540
ROWS = (('A', 500), ('B', 700), ('C', 900), ('D', 1100), ('F', 1300), ('S', 1500))


def count(colour):
    """The count a colour shows (n -> (n % 256, n // 256 x 60 + 20, 7n % 256)), or None."""
    r, g, b = colour[:3]
    high = round((g - 20) / 60)
    n = high * 256 + r
    return n if 0 <= high < 4 and abs(g - (high * 60 + 20)) < 12 and abs(b - (n * 7) % 256) < 12 else None


def read(path):
    picture = Image.open(path).convert('RGB')
    width, height = picture.size
    values = {}
    for name, x in ROWS:
        values[name] = count(picture.getpixel((int(x / 1920 * width), int(540 / 1080 * height))))
    return clock_probe.read(picture), clock_probe.read_script(picture), values


if __name__ == '__main__':
    for path in sys.argv[1:]:
        frame, script, values = read(path)
        print('%s frame %s script %s %s' % (path, frame, script, ' '.join('%s %s' % kv for kv in values.items())))

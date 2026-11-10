#!/usr/bin/env python3
"""Reads texture-animation-created-late pictures: the probe's clock (g_Time, frames of 1/30 s) and each layer's sheet frame
(its colour; frames of 0.03 s), and when that frame says the layer's sheet started. Usage: texture_animation_created_read.py PICTURE"""
import importlib.util
import os
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location('clock_probe', os.path.join(HERE, 'clock-probe.py'))
clock_probe = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(clock_probe)
FRAMES, FRAMETIME = 1000, 0.03
PALETTE = [(i % 256, (i // 256) * 60 + 20, (i * 7) % 256) for i in range(FRAMES)]


def frame_at(picture, x):
    w, h = picture.size
    s = w / 1920
    pixels = sorted(picture.getpixel((int((x + dx) * s), int((540 + dy) * s)))[:3]
                    for dx in range(-60, 61, 20) for dy in range(-60, 61, 20))
    colour = pixels[len(pixels) // 2]
    best = min(range(FRAMES), key=lambda i: sum(abs(p - q) for p, q in zip(PALETTE[i], colour)))
    return best, sum(abs(p - q) for p, q in zip(PALETTE[best], colour))


if __name__ == '__main__':
    picture = Image.open(sys.argv[1]).convert('RGB')
    clock = clock_probe.read(picture)
    t = clock / 30 if clock is not None else None
    print('g_Time frame %s (%.3f s)' % (clock, t if t is not None else float('nan')))
    for label, x in (('start', 300), ('2 s', 740), ('7 s', 1180), ('12 s', 1620)):
        frame, distance = frame_at(picture, x)
        started = None if t is None else t - frame * FRAMETIME
        print('%-6s frame %4d (distance %d)  started at %s' % (label, frame, distance,
                                                             '%.2f s' % started if started is not None else '?'))

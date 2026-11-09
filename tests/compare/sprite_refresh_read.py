#!/usr/bin/env python3
"""Reads a sprite-refresh picture (make/sprite-refresh.py): the probe's scene clock (g_Time, frames of 1/30 s) and the sheet's
frame (its colour), and what the frame would be if the sheet played at its frames' rate or at one frame an update.
Usage: sprite_refresh_read.py PICTURE FRAMETIME FPS"""
import importlib.util
import os
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location('clock_probe', os.path.join(HERE, 'clock-probe.py'))
clock_probe = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(clock_probe)

FRAMES = 1000


def color(i):
    return (i % 256, (i // 256) * 60 + 20, (i * 7) % 256)


PALETTE = [color(i) for i in range(FRAMES)]


def sheet_frame(picture, x=0.5):
    w, h = picture.size
    pixels = sorted(picture.getpixel((int(w * x) + dx, h // 2 + dy))[:3] for dx in range(-40, 41, 10) for dy in range(-40, 41, 10))
    r, g, b = pixels[len(pixels) // 2]
    best = min(range(FRAMES), key=lambda i: sum(abs(p - q) for p, q in zip(PALETTE[i], (r, g, b))))
    return best, sum(abs(p - q) for p, q in zip(PALETTE[best], (r, g, b)))


def read(path, frametime, fps):
    picture = Image.open(path).convert('RGB')
    clock = clock_probe.read(picture)
    frame, distance = sheet_frame(picture)
    updates_seen, udistance = sheet_frame(picture, 300 / 1920)
    seconds = clock / 30.0 if clock is not None else None
    line = '%-40s g_Time %s  sheet frame %4d%s' % (
        os.path.basename(os.path.dirname(path)) + '/' + os.path.basename(path),
        '%7.3f s' % seconds if seconds is not None else '   ?   ', frame, '' if distance < 12 else ' (colour off by %d)' % distance)
    if seconds is not None:
        rate = seconds / frametime
        updates = seconds * fps
        line += '  | updates %4d (%.1f a second)%s' % (updates_seen, updates_seen / seconds if seconds else 0,
                                                     '' if udistance < 12 else ' (off by %d)' % udistance)
        line += '  | frames rate %6.1f  one per update %6.1f  | sheet time %.3f s = %.3f x g_Time' % (
            rate % FRAMES, updates % FRAMES, frame * frametime, frame * frametime / seconds if seconds else 0)
    print(line)
    return seconds, frame


if __name__ == '__main__':
    read(sys.argv[1], float(sys.argv[2]), float(sys.argv[3]))

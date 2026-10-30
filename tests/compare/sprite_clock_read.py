#!/usr/bin/env python3
"""Reads the sprite times of the layers of a sprite-clock picture (make/sprite-clock.py): each layer's colour is a frame of a
300-frame sheet of 0.1 s frames (30 s loop). Prints, for the picture (reference.png, owe.png): the scene clock (the probe's
frames), and for each of the three layers its frame and sprite time, and what it would be if the animation had started with the
scene (the control) or at its creation (2.0 s and 3.5 s)."""
import os
import sys

from PIL import Image

FRAMES = 300


def color(i):
    return (i % 256, (i // 256) * 100 + 50, (i * 3) % 256)


PALETTE = [color(i) for i in range(FRAMES)]
# the layers' centres in a 1600 x 900 picture of a 1920 x 1080 scene
CENTRES = (('from the start', 500, 0.0), ('created at 2.0 s', 1000, 2.0), ('created at 3.5 s', 1500, 3.5))


def frame_of(picture, x, y):
    pixels = [picture.getpixel((x + dx, y + dy))[:3] for dx in range(-6, 7, 3) for dy in range(-6, 7, 3)]
    pixels.sort()
    r, g, b = pixels[len(pixels) // 2]
    best = min(range(FRAMES), key=lambda i: abs(PALETTE[i][0] - r) + abs(PALETTE[i][1] - g) + abs(PALETTE[i][2] - b))
    distance = abs(PALETTE[best][0] - r) + abs(PALETTE[best][1] - g) + abs(PALETTE[best][2] - b)
    return best, distance


def read(path, scene_seconds=None):
    picture = Image.open(path).convert('RGB')
    scale = picture.size[0] / 1920.0
    print(os.path.basename(os.path.dirname(path)) + '/' + os.path.basename(path), picture.size,
          'scene clock %s s' % scene_seconds if scene_seconds is not None else '')
    for label, x, created in CENTRES:
        frame, distance = frame_of(picture, int(x * scale), int(540 * scale))
        shown = frame * 0.1
        line = '  %-17s frame %3d  sprite time %5.1f s%s' % (label, frame, shown, '' if distance < 12 else '  (colour off by %d)' % distance)
        if scene_seconds is not None:
            expected = (scene_seconds - created) % 30.0
            line += '   expected from its start %5.1f s   (off by %+.1f s)' % (expected, ((shown - expected + 15) % 30) - 15)
        print(line)


if __name__ == '__main__':
    seconds = float(sys.argv[2]) if len(sys.argv) > 2 else None
    read(sys.argv[1], seconds)

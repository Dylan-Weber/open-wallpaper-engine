"""Reads the record a web probe page (make/web-probe.py) paints at its top left: what the page was given by the engine
(the audio listener's 128 values, user properties, window size and pixel ratio).

    python3 web_record.py PICTURE ...
"""
import json
import sys

from PIL import Image

MAGIC = 0xA5C3
CELL, PER_ROW, X0, Y0 = 6, 64, 16, 16
FIELDS = [('magic', 16), ('width', 16), ('height', 16), ('pixel_ratio', 16), ('calls_per_second', 16), ('calls', 16),
          ('values', 16), ('property_calls', 16), ('red', 8), ('green', 8), ('blue', 8), ('number', 8), ('flag', 16),
          ('general_calls', 8), ('fps', 8), ('paused_calls', 8), ('paused', 8)] + \
         [('audio%d' % i, 16) for i in range(128)]


def read_bits(picture, count, scale=1.0):
    pixels = picture.load()
    bits = []
    for b in range(count):
        x = (X0 + (b % PER_ROW) * CELL + CELL / 2) * scale
        y = (Y0 + (b // PER_ROW) * CELL + CELL / 2) * scale
        bits.append(1 if sum(pixels[int(x), int(y)][:3]) > 3 * 127 else 0)
    return bits


def read(picture):
    """The record's fields, or None when the picture has none. A page drawn at a pixel ratio other than 1 is read at
    that scale."""
    picture = picture.convert('RGB')
    total = sum(n for _, n in FIELDS)
    for scale in (1.0, 1.25, 1.5, 2.0, 0.8333):
        bits = read_bits(picture, total, scale)
        fields, at = {}, 0
        for name, n in FIELDS:
            fields[name] = int(''.join(map(str, bits[at:at + n])), 2)
            at += n
        if fields['magic'] == MAGIC:
            audio = [fields.pop('audio%d' % i) / 4096 for i in range(128)]
            fields.pop('magic')
            fields['pixel_ratio'] /= 1000
            fields['left'], fields['right'] = audio[:64], audio[64:]
            fields['scale'] = scale
            return fields
    return None


if __name__ == '__main__':
    for path in sys.argv[1:]:
        print(path, json.dumps(read(Image.open(path))))

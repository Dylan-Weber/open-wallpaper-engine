# layer-size-fraction: what Wallpaper Engine does with an image layer's size below 1 or not a whole number
# (texture-uv-inset-size: a layer of size 2 x 0.5 isn't drawn at all). The same 4-texel texture drawn 280 x 12 scene units
# (343 x 193 scene) by layers of size 4 x 0.5, 4 x 0.99, 4 x 1, 4 x 1.49, 4 x 1.5 and 0.5 x 1, 0.99 x 1 (scaled to the same
# drawn size): a size below 1 might count as empty, and a fractional size might be truncated (drawn smaller) or rounded.
import struct


def texture(width):
    pixels = bytearray()
    for x in range(width):
        pixels += bytes((64 * x + 32, 255, 255, 255))
    data = b'TEXV0005\0TEXI0001\0' + struct.pack('<7I', binary.RGBA8888, 3, width, 1, width, 1, 0)
    data += b'TEXB0003\0' + struct.pack('<Ii', 1, -1) + struct.pack('<I', 1)
    data += struct.pack('<IIII', width, 1, 0, len(pixels)) + struct.pack('<I', len(pixels)) + bytes(pixels)
    return data


w = Wallpaper('layer-size-fraction', 'image layers of fractional size', clear='0.1 0.1 0.15', size=(343, 193))
write_binary(os.path.join(w.name, 'materials', 'tex4.tex'), texture(4))
four = w.texture_model('tex4', 'tex4')
ROWS = ((4, 0.5), (4, 0.99), (4, 1), (4, 1.49), (4, 1.5), (0.5, 1), (0.99, 1))
# above the probe's records (the bottom fifth of the screen), 18 apart
for k, (width, height) in enumerate(ROWS):
    w.image('size %g x %g' % (width, height), four, 171.5, 45 + 18 * k, width, height,
            scale='%g %g 1' % (280 / width, 12 / height))
w.save()
with open(os.path.join(HERE, w.name, 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, w.name))

# the same sizes as solid-colour layers (models/util/solidlayer.json: an image layer without a texture of its own)
w = Wallpaper('layer-size-fraction-solid', 'solid layers of fractional size', clear='0.1 0.1 0.15', size=(343, 193))
for k, (width, height) in enumerate(ROWS):
    w.add({"name": 'solid %g x %g' % (width, height), "image": "models/util/solidlayer.json",
           "size": '%g %g' % (width, height), "color": "0.2 0.9 0.9", "alpha": 1, "angles": "0 0 0",
           "scale": '%g %g 1' % (280 / width, 12 / height), "alignment": "center", "visible": True,
           "origin": '171.5 %g 0' % (45 + 18 * k)})
w.save()
with open(os.path.join(HERE, w.name, 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, w.name))

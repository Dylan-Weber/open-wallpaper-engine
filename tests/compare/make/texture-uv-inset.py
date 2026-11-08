# texture-uv-inset: which textures Wallpaper Engine samples a little zoomed. In layer-wide-scale its texels were 0.9 %
# wider apart than the layer's size says (at every scale, the layer's edges where they belong), for a 32 x 32 texture
# without interpolation and with clamped coordinates (flags 3, like Dino Run's grass). Each row here is a layer of the
# same size (scaled 10 x 0.3, 320 wide in a 343 x 193 scene) whose texture has N columns, each its own colour (red
# 256 / N x column + 128 / N): bottom up, 32 columns with the texture flags 0 (interpolated, wrapping), 1 (no
# interpolation), 2 (clamped) and 3 (both), then 16, 64 and 128 columns with flags 3.
import struct


def column_texture(size, flags):
    pixels = bytearray()
    for y in range(size):
        for x in range(size):
            pixels += bytes((256 // size * x + 128 // size, 255 if y < size // 2 else 128, (x % 2) * 255, 255))
    data = b'TEXV0005\0TEXI0001\0' + struct.pack('<7I', binary.RGBA8888, flags, size, size, size, size, 0)
    data += b'TEXB0003\0' + struct.pack('<Ii', 1, -1) + struct.pack('<I', 1)
    data += struct.pack('<IIII', size, size, 0, len(pixels)) + struct.pack('<I', len(pixels)) + bytes(pixels)
    return data


w = Wallpaper('texture-uv-inset', 'textures sampled a little zoomed', clear='0.1 0.1 0.15', size=(343, 193))
ROWS = ((32, 0), (32, 1), (32, 2), (32, 3), (16, 3), (64, 3), (128, 3))
for k, (size, flags) in enumerate(ROWS):
    name = 'columns%d_%d' % (size, flags)
    write_binary(os.path.join(w.name, 'materials', name + '.tex'), column_texture(size, flags))
    model = w.texture_model(name, name, model_extra={"autosize": True})
    # above the probe's records (the bottom fifth of the screen), 17 apart; every layer 320 x 9.6
    w.image('%d columns, flags %d' % (size, flags), model, 171.5, 50 + 17 * k, size, size,
            scale='%g %g 1' % (320 / size, 9.6 / size))
w.save()
with open(os.path.join(HERE, w.name, 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, w.name))

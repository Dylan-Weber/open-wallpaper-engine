# texture-uv-inset-big: how far in from its edges Wallpaper Engine starts sampling a layer's texture. texture-uv-inset
# showed the texels of every texture drawn about 0.13 texel short at each end (a 32-texel texture 0.9 % wide), so
# tiny textures blown up show it most: rows of a 4- and an 8-texel texture 320 wide (a texel 373 and 187 screen pixels
# at 1600 x 900), the 4-texel one again with interpolation (flags 2), and a tall layer of a texture with 4 texel rows
# for the vertical. Column x of a texture is red 256 / N x x + 128 / N, row y green 64 y + 32.
import struct


def texture(columns, rows, flags):
    pixels = bytearray()
    for y in range(rows):
        for x in range(columns):
            pixels += bytes((256 // columns * x + 128 // columns, 64 * y + 32 if rows > 1 else 255, 255, 255))
    data = b'TEXV0005\0TEXI0001\0' + struct.pack('<7I', binary.RGBA8888, flags, columns, rows, columns, rows, 0)
    data += b'TEXB0003\0' + struct.pack('<Ii', 1, -1) + struct.pack('<I', 1)
    data += struct.pack('<IIII', columns, rows, 0, len(pixels)) + struct.pack('<I', len(pixels)) + bytes(pixels)
    return data


w = Wallpaper('texture-uv-inset-big', 'how far in a layer\'s texture starts', clear='0.1 0.1 0.15', size=(343, 193))
ROWS = (('4 columns', 4, 1, 3), ('8 columns', 8, 1, 3), ('4 columns, interpolated', 4, 1, 2))
for k, (label, columns, rows, flags) in enumerate(ROWS):
    name = 'tex%dx%d_%d' % (columns, rows, flags)
    write_binary(os.path.join(w.name, 'materials', name + '.tex'), texture(columns, rows, flags))
    model = w.texture_model(name, name, model_extra={"autosize": True})
    # above the probe's records (the bottom fifth of the screen), 20 apart; 300 x 12, left of the tall layer
    w.image(label, model, 150, 60 + 20 * k, columns, rows, scale='%g %g 1' % (280 / columns, 12 / rows))
name = 'tex1x4_3'
write_binary(os.path.join(w.name, 'materials', name + '.tex'), texture(1, 4, 3))
model = w.texture_model(name, name, model_extra={"autosize": True})
w.image('4 rows', model, 315, 110, 1, 4, scale='%g %g 1' % (30, 120 / 4))
w.save()
with open(os.path.join(HERE, w.name, 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, w.name))

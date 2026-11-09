# layer-wide-scale: an image layer wider than the scene. In effect-repeat-scaled, Wallpaper Engine drew the 32 x 32
# layer scaled 10.821 (346.27 wide in a 343 wide scene) with its texels 0.92 % wider apart than its size says (50.93
# screen pixels against 50.47 at 1600 x 900, centred), with or without an effect, while layers narrower than the scene
# matched. Dino Run's grass is such a layer. Rows of the same 32 x 32 texture (every column its own colour, red 8 x
# column + 4, no interpolation), bottom up, scaled across 9, 10.5, 10.821, 11, 12 and 16 (288 to 512 wide), then a
# layer 346.27 x 32 at scale 1 (wide by its size) and one scaled 10.821 in a group scaled 0.5 (173 wide on screen).
import struct

SIZE = 32


def column_texture():
    pixels = bytearray()
    for y in range(SIZE):
        for x in range(SIZE):
            pixels += bytes((8 * x + 4, 255 if y < SIZE // 2 else 128, (x % 2) * 255, 255))
    # flags 3: no interpolation, clamped coordinates (Dino Run's grass)
    data = b'TEXV0005\0TEXI0001\0' + struct.pack('<7I', binary.RGBA8888, 3, SIZE, SIZE, SIZE, SIZE, 0)
    data += b'TEXB0003\0' + struct.pack('<Ii', 1, -1) + struct.pack('<I', 1)
    data += struct.pack('<IIII', SIZE, SIZE, 0, len(pixels)) + struct.pack('<I', len(pixels)) + bytes(pixels)
    return data


w = Wallpaper('layer-wide-scale', 'image layers wider than the scene', clear='0.1 0.1 0.15', size=(343, 193))
write_binary(os.path.join(w.name, 'materials', 'columns.tex'), column_texture())
model = w.texture_model('columns', 'columns', model_extra={"autosize": True})
# above the probe's records (the bottom fifth of the screen), 16 apart
for k, scale in enumerate((9, 10.5, 10.821, 11, 12, 16)):
    w.image('scale %g' % scale, model, 171.5, 50 + 16 * k, SIZE, SIZE, scale='%g 0.3 1' % scale)
w.image('size 346.27', model, 171.5, 146, 346.27, SIZE, scale='1 0.3 1')
group = w.group('half', 171.5, 162, scale='0.5 1 1')
w.image('scale 10.821 in half', model, 0, 0, SIZE, SIZE, scale='10.821 0.3 1', parent=group)
w.save()
with open(os.path.join(HERE, w.name, 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, w.name))

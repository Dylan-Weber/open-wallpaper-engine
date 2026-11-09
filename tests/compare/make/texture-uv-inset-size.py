# texture-uv-inset-size: is Wallpaper Engine's texture inset (texture-uv-inset-big: a layer's texels start about 0.15
# texel in from its edges) counted in the texture's texels or in the layer's size? Rows of the same 4-texel texture
# drawn 280 x 12 (scene units, 343 x 193 scene) by layers of size 4 x 1, 8 x 2 and 2 x 0.5 (scaled to the same width),
# then a texture of 3 texels padded to 4 (the texture 4 wide, the image 3: owe's g_Texture0Resolution maps 0.75 of
# it) on a layer of size 3 x 1. Texel x is red 64 x + 32 (red 255 marks the padding).
import struct


def texture(width, image_width, flags=3):
    pixels = bytearray()
    for x in range(width):
        pixels += bytes((64 * x + 32 if x < image_width else 255, 255, 255, 255))
    data = b'TEXV0005\0TEXI0001\0' + struct.pack('<7I', binary.RGBA8888, flags, width, 1, image_width, 1, 0)
    data += b'TEXB0003\0' + struct.pack('<Ii', 1, -1) + struct.pack('<I', 1)
    data += struct.pack('<IIII', width, 1, 0, len(pixels)) + struct.pack('<I', len(pixels)) + bytes(pixels)
    return data


w = Wallpaper('texture-uv-inset-size', 'a texture inset in texels or in layer units', clear='0.1 0.1 0.15',
              size=(343, 193))
write_binary(os.path.join(w.name, 'materials', 'tex4.tex'), texture(4, 4))
write_binary(os.path.join(w.name, 'materials', 'tex3of4.tex'), texture(4, 3))
four = w.texture_model('tex4', 'tex4')
padded = w.texture_model('tex3of4', 'tex3of4')
ROWS = (('size 4', four, 4, 1), ('size 8', four, 8, 2), ('size 2', four, 2, 0.5), ('padded 3 of 4', padded, 3, 1))
# above the probe's records (the bottom fifth of the screen), 25 apart
for k, (label, model, width, height) in enumerate(ROWS):
    w.image(label, model, 171.5, 60 + 25 * k, width, height, scale='%g %g 1' % (280 / width, 12 / height))
w.save()
with open(os.path.join(HERE, w.name, 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, w.name))

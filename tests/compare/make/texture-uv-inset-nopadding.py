# texture-uv-inset-nopadding: does a model's "nopadding" switch off Wallpaper Engine's texture inset (texture-uv-inset-big:
# a layer's texels start 0.15 texel in from its edges)? The videotex-* probes (video textures, flags 35, on models with
# "nopadding": true) match Wallpaper Engine only without the inset. Rows of the same 4-texel texture (flags 3, column x red
# 64 x + 32) 280 x 12 in a 343 x 193 scene: bottom a plain model, top a model with "nopadding": true.
import struct


def texture():
    pixels = bytearray()
    for x in range(4):
        pixels += bytes((64 * x + 32, 255, 255, 255))
    data = b'TEXV0005\0TEXI0001\0' + struct.pack('<7I', binary.RGBA8888, 3, 4, 1, 4, 1, 0)
    data += b'TEXB0003\0' + struct.pack('<Ii', 1, -1) + struct.pack('<I', 1)
    data += struct.pack('<IIII', 4, 1, 0, len(pixels)) + struct.pack('<I', len(pixels)) + bytes(pixels)
    return data


w = Wallpaper('texture-uv-inset-nopadding', 'a model\'s nopadding and the texture inset', clear='0.1 0.1 0.15',
              size=(343, 193))
write_binary(os.path.join(w.name, 'materials', 'tex4.tex'), texture())
plain = w.texture_model('plain', 'tex4')
nopadding = w.texture_model('nopadding', 'tex4', model_extra={"nopadding": True})
# above the probe's records (the bottom fifth of the screen)
w.image('plain', plain, 171.5, 70, 4, 1, scale='70 12 1')
w.image('nopadding', nopadding, 171.5, 110, 4, 1, scale='70 12 1')
w.save()
with open(os.path.join(HERE, w.name, 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, w.name))

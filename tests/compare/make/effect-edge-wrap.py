# effect-edge-wrap: what an effect reads past its layer's edge (ws2759157886: the refraction effect pulls a cyan streak
# from the bottom of the picture to its top edge in owe, not in Wallpaper Engine). A picture whose top quarter is red and
# bottom quarter cyan (grey between); the shipped refraction effect at strength 0.3 pushes the texture coordinates past
# the edges: wrapping shows cyan at the top and red at the bottom, clamping shows red at the top and cyan at the bottom.
# Cells: refraction 0.3, refraction 0 (control), tint then refraction 0.3 (a target in between), and the picture alone.
import struct
SHIPPED_PASSES['refraction'] = 2


def bands_tex(side=256):
    pixels = bytearray()
    for y in range(side):
        color = (255, 40, 40) if y < side // 4 else (40, 230, 230) if y >= side - side // 4 else (60, 60, 60)
        pixels += bytes(color + (255,)) * side
    data = b'TEXV0005\0TEXI0001\0' + struct.pack('<7I', binary.RGBA8888, 3, side, side, side, side, 0)
    data += b'TEXB0003\0' + struct.pack('<Ii', 1, -1) + struct.pack('<I', 1)
    data += struct.pack('<IIII', side, side, 0, len(pixels)) + struct.pack('<I', len(pixels)) + bytes(pixels)
    return data


w = Wallpaper('effect-edge-wrap', 'what an effect reads past the edge of its layer', clear='0.12 0.12 0.12')
write_binary(os.path.join(w.name, 'materials', 'bands.tex'), bands_tex())
model = w.texture_model('picture', 'bands')
cells = [('refraction 0.3', [shipped('refraction', {"strength": 0.3})]),
         ('refraction 0', [shipped('refraction', {"strength": 0})]),
         ('tint, refraction 0.3', [shipped('tint', {"color": "1 1 1", "blendmode": 0}), shipped('refraction', {"strength": 0.3})]),
         ('plain', [])]
for i, (label, effects) in enumerate(cells):
    effects = [dict(e, id=100 + i * 10 + k, passes=[dict(p, id=1000 + i * 10 + k * 3 + n) for n, p in enumerate(e['passes'])])
               for k, e in enumerate(effects)]
    w.image(label, model, 260 + i * 460, 540, 256, 256, scale='1.4 1.4 1', effects=effects)
w.save()
copy_shipped_effects(w, ['refraction', 'tint'])
with open(os.path.join(HERE, w.name, 'compare.json'), 'w') as f:
    json.dump({"probe": True, "fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, w.name))

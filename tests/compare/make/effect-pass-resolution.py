# effect-pass-resolution: at what resolution Wallpaper Engine runs an effect's passes on a layer drawn larger than its
# texture. effect-repeat-scaled showed a one-pass effect (frac(uv * repeat)) on a 32 x 32 layer scaled 5 resolving every
# texel on screen, where owe ran it in a 32 x 32 target and stretched it. Rows (each a 32 x 32 layer of a texture whose
# every column has its own colour, red 8 x column + 4, no interpolation, scaled 9 x 0.4: 288 x 12.8 in a 343 x 193
# scene), bottom up: the one-pass effect; the same work in a first pass into the layer's next buffer and a second
# pass copying it ("previous"); the same into an effect target of its own ("_rt_Work", default size) copied by a
# second pass; and two effects in a chain, the repeat first and a copy effect after it.
import struct

SIZE = 32
FRAG_REPEAT = """varying vec4 v_TexCoord;
uniform sampler2D g_Texture0; // {"hidden":true}
void main() {
	gl_FragColor = texSample2D(g_Texture0, frac(v_TexCoord.xy * vec2(7.0, 1.0)));
}
"""
FRAG_COPY = """varying vec4 v_TexCoord;
uniform sampler2D g_Texture0; // {"hidden":true}
void main() {
	gl_FragColor = texSample2D(g_Texture0, v_TexCoord.xy);
}
"""


def column_texture():
    pixels = bytearray()
    for y in range(SIZE):
        for x in range(SIZE):
            pixels += bytes((8 * x + 4, 255, (x % 2) * 255, 255))
    data = b'TEXV0005\0TEXI0001\0' + struct.pack('<7I', binary.RGBA8888, 3, SIZE, SIZE, SIZE, SIZE, 0)
    data += b'TEXB0003\0' + struct.pack('<Ii', 1, -1) + struct.pack('<I', 1)
    data += struct.pack('<IIII', SIZE, SIZE, 0, len(pixels)) + struct.pack('<I', len(pixels)) + bytes(pixels)
    return data


w = Wallpaper('effect-pass-resolution', 'the resolution of an effect\'s passes on an upscaled layer',
              clear='0.1 0.1 0.15', size=(343, 193))
write_binary(os.path.join(w.name, 'materials', 'columns.tex'), column_texture())
model = w.texture_model('columns', 'columns', model_extra={"autosize": True})
for name, frag in (('repeat', FRAG_REPEAT), ('copy', FRAG_COPY)):
    w.files['materials/effects/%s.json' % name] = {"passes": [{
        "shader": "effects/" + name, "blending": "normal", "depthtest": "disabled", "depthwrite": "disabled",
        "cullmode": "nocull"}]}
    w.files['shaders/effects/%s.vert' % name] = VERT
    w.files['shaders/effects/%s.frag' % name] = frag
EFFECTS = {
    'repeat': {"passes": [{"material": "materials/effects/repeat.json"}]},
    'copy': {"passes": [{"material": "materials/effects/copy.json"}]},
    'twopass': {"passes": [{"material": "materials/effects/repeat.json"},
                           {"material": "materials/effects/copy.json"}]},
    'ownfbo': {"passes": [{"material": "materials/effects/repeat.json", "target": "_rt_Work"},
                          {"material": "materials/effects/copy.json", "bind": [{"name": "_rt_Work", "index": 0}]}],
               "fbos": [{"name": "_rt_Work", "format": "rgba8888"}]},
}
for name, effect in EFFECTS.items():
    w.files['effects/%s/effect.json' % name] = dict({"version": 1, "name": name}, **effect)


def instance(name, id):
    return {"file": "effects/%s/effect.json" % name, "id": id, "name": name, "visible": True,
            "passes": [{"id": id * 10 + n} for n in range(len(EFFECTS[name]["passes"]))]}


ROWS = (('one pass', ['repeat']), ('two passes', ['twopass']), ('own target', ['ownfbo']),
        ('two effects', ['repeat', 'copy']))
for k, (label, names) in enumerate(ROWS):
    # above the probe's records (the bottom fifth of the screen), 25 apart
    w.image(label, model, 171.5, 60 + 25 * k, SIZE, SIZE, scale='9 0.4 1',
            effects=[instance(name, 10 * k + j + 1) for j, name in enumerate(names)])
w.save()
with open(os.path.join(HERE, w.name, 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, w.name))

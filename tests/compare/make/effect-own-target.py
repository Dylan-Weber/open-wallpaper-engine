# effect-own-target: why Wallpaper Engine hid effect-pass-chain's layers whose effect writes its first pass into a target of
# its own ("_rt_Work") and copies it in a second, bound pass, while effect-passes (the same structure: "_rt_BlurTemp",
# rgba_backbuffer, no scale) draws. Rows, bottom up, each the same repeat-then-copy effect on a 32 x 32 layer scaled 9 x 0.4
# in a 343 x 193 scene: the effect exactly as in effect-pass-chain (control, point-sampled 32-column texture); the target
# named "_rt_BlurTemp"; the effect with material files of its own; the control effect on a layer of the shipped clouds
# texture (interpolated, not autosized).
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


w = Wallpaper('effect-own-target', 'an effect writing into a target of its own', clear='0.1 0.1 0.15', size=(343, 193))
write_binary(os.path.join(w.name, 'materials', 'columns.tex'), column_texture())
columns = w.texture_model('columns', 'columns', model_extra={"autosize": True})
clouds = w.texture_model('clouds', 'util/clouds_256')
for name, frag in (('repeat', FRAG_REPEAT), ('copy', FRAG_COPY), ('repeat_own', FRAG_REPEAT), ('copy_own', FRAG_COPY)):
    w.files['materials/effects/%s.json' % name] = {"passes": [{
        "shader": "effects/" + name, "blending": "normal", "depthtest": "disabled", "depthwrite": "disabled",
        "cullmode": "nocull"}]}
    w.files['shaders/effects/%s.vert' % name] = VERT
    w.files['shaders/effects/%s.frag' % name] = frag


def effect(name, target, repeat='repeat', copy='copy'):
    w.files['effects/%s/effect.json' % name] = {
        "version": 1, "name": name,
        "passes": [{"material": "materials/effects/%s.json" % repeat, "target": target},
                   {"material": "materials/effects/%s.json" % copy, "bind": [{"name": target, "index": 0}]}],
        "fbos": [{"name": target, "format": "rgba_backbuffer"}]}


effect('control', '_rt_Work')
effect('blurtemp', '_rt_BlurTemp')
effect('ownmaterials', '_rt_Work', 'repeat_own', 'copy_own')
effect('clouds', '_rt_Work')
ROWS = (('control', columns), ('blurtemp', columns), ('ownmaterials', columns), ('clouds', clouds))
for k, (name, model) in enumerate(ROWS):
    # above the probe's records (the bottom fifth of the screen), 25 apart
    w.image(name, model, 171.5, 60 + 25 * k, SIZE, SIZE, scale='9 0.4 1',
            effects=[{"file": "effects/%s/effect.json" % name, "id": 10 * k + 1, "name": name, "visible": True,
                      "passes": [{"id": 100 * (k + 1)}, {"id": 100 * (k + 1) + 1}]}])
w.save()
with open(os.path.join(HERE, w.name, 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, w.name))

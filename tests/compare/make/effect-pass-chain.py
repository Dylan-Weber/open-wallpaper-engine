# effect-pass-chain: what a later pass of an effect samples, and when an effect's own target exists. In
# effect-pass-resolution Wallpaper Engine drew a two-pass effect (a repeat, then a copy, no targets or binds) as the plain
# texture, and hid a layer whose effect wrote into its own target declared with only "format": "rgba8888". Shipped effects
# suggest why: refraction's first pass is "compose": true and its unbound second pass sees it; blurprecise's target has
# only "format": "rgba_backbuffer" and works. Rows (a 32 x 32 texture whose every column has its own colour, red 8 x
# column + 4, scaled 9 x 0.4 in a 343 x 193 scene), bottom up: repeat then copy; the same with the repeat pass
# "compose": true; the repeat into "_rt_Work" (format rgba8888, no size) copied by a bound second pass; the same with
# format rgba_backbuffer; the same with rgba8888 and "scale": 1.
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


w = Wallpaper('effect-pass-chain', 'what an effect\'s later pass samples, and its own targets', clear='0.1 0.1 0.15',
              size=(343, 193))
write_binary(os.path.join(w.name, 'materials', 'columns.tex'), column_texture())
model = w.texture_model('columns', 'columns', model_extra={"autosize": True})
for name, frag in (('repeat', FRAG_REPEAT), ('copy', FRAG_COPY)):
    w.files['materials/effects/%s.json' % name] = {"passes": [{
        "shader": "effects/" + name, "blending": "normal", "depthtest": "disabled", "depthwrite": "disabled",
        "cullmode": "nocull"}]}
    w.files['shaders/effects/%s.vert' % name] = VERT
    w.files['shaders/effects/%s.frag' % name] = frag
REPEAT = {"material": "materials/effects/repeat.json"}
COPY = {"material": "materials/effects/copy.json"}
BOUND_COPY = {"material": "materials/effects/copy.json", "bind": [{"name": "_rt_Work", "index": 0}]}
EFFECTS = (
    ('chain', [REPEAT, COPY], None),
    ('compose', [dict(REPEAT, compose=True), COPY], None),
    ('fbo8888', [dict(REPEAT, target='_rt_Work'), BOUND_COPY], {"name": "_rt_Work", "format": "rgba8888"}),
    ('fbobackbuffer', [dict(REPEAT, target='_rt_Work'), BOUND_COPY], {"name": "_rt_Work", "format": "rgba_backbuffer"}),
    ('fbo8888scale', [dict(REPEAT, target='_rt_Work'), BOUND_COPY],
     {"name": "_rt_Work", "format": "rgba8888", "scale": 1}),
)
for k, (name, passes, fbo) in enumerate(EFFECTS):
    effect = {"version": 1, "name": name, "passes": passes}
    if fbo:
        effect["fbos"] = [fbo]
    w.files['effects/%s/effect.json' % name] = effect
    # above the probe's records (the bottom fifth of the screen), 22 apart
    w.image(name, model, 171.5, 55 + 22 * k, SIZE, SIZE, scale='9 0.4 1',
            effects=[{"file": "effects/%s/effect.json" % name, "id": 10 * k + 1, "name": name, "visible": True,
                      "passes": [{"id": 100 * (k + 1) + n} for n in range(len(passes))]}])
w.save()
with open(os.path.join(HERE, w.name, 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, w.name))

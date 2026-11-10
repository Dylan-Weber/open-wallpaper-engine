# effect-fbo-scale-*: which effect targets make Wallpaper Engine leave a layer out (effect-pass-resolution's "own target" row,
# an effect drawing into a target of its own with no "scale", isn't drawn at all). One wallpaper each (a scene with a scale 0 target drew nothing at all), the two-pass effect
# (a repeat into _rt_Work, then a copy of it): the target with no scale, scale 0, scale 1, fit 64 and no scale, and width
# and height 64 and no scale.
# (setup from effect-pass-resolution) at what resolution Wallpaper Engine runs an effect's passes on a layer drawn larger than its
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


VARIANTS = (('noscale', {}), ('scale0', {"scale": 0}), ('scale1', {"scale": 1}), ('fit', {"fit": 64}),
            ('size', {"width": 64, "height": 64}))


def make(label, fbo, first_bind=None, target_name='_rt_Work', last_previous=False, repeat_vert=None):
    wname = 'effect-fbo-scale-' + label
    w = Wallpaper(wname, 'an effect target: %s' % label,
                  clear='0.1 0.1 0.15', size=(343, 193))
    write_binary(os.path.join(w.name, 'materials', 'columns.tex'), column_texture())
    model = w.texture_model('columns', 'columns', model_extra={"autosize": True})
    for name, frag in (('repeat', FRAG_REPEAT), ('copy', FRAG_COPY)):
        w.files['materials/effects/%s.json' % name] = {"passes": [{
            "shader": "effects/" + name, "blending": "normal", "depthtest": "disabled", "depthwrite": "disabled",
            "cullmode": "nocull"}]}
        w.files['shaders/effects/%s.vert' % name] = repeat_vert if name == 'repeat' and repeat_vert else VERT
        w.files['shaders/effects/%s.frag' % name] = frag

    first = {"material": "materials/effects/repeat.json", "target": target_name}
    if first_bind:
        first["bind"] = [{"name": first_bind, "index": 0}]
    effect = {"passes": [first,
                         {"material": "materials/effects/copy.json", "bind": [{"name": target_name, "index": 0}] + (
                             [{"name": "previous", "index": 2}] if last_previous else [])}],
              "fbos": [dict({"name": target_name, "format": "rgba8888"}, **fbo)]}
    w.files['effects/target/effect.json'] = dict({"version": 1, "name": "target"}, **effect)
    w.image('layer', model, 171.5, 100, SIZE, SIZE, scale='9 0.4 1',
            effects=[{"file": "effects/target/effect.json", "id": 11, "name": "target", "visible": True,
                      "passes": [{"id": 100 + n} for n in range(2)]}])
    w.save()
    with open(os.path.join(HERE, wname, 'compare.json'), 'w') as f:
        json.dump({"fps": 30}, f)
    clock_probe.stamp(os.path.join(HERE, wname))


for label, fbo in VARIANTS:
    make(label, fbo)
# scale 1 with the first pass binding "previous" (as shipped effects do)
make('bindprev', {"scale": 1}, 'previous')
# the shipped effects' target name, and their last pass binding "previous" too
make('compobuffer', {"scale": 1}, 'previous', '_rt_FullCompoBuffer1')
make('lastprevious', {"scale": 1}, 'previous', '_rt_Work', True)
# the first pass placing its quad as shipped passes into a target do (blur_downsample4.vert): no matrix
make('identvert', {"scale": 1}, 'previous', repeat_vert=VERT.replace(
    'mul(vec4(a_Position, 1.0), g_ModelViewProjectionMatrix)', 'vec4(a_Position, 1.0)'))

# effect-repeat-scaled: where an effect's texture coordinates fall on a layer scaled unevenly. Dino Run's grass is a
# 32 x 32 texture (no interpolation, clamped) on a 32 x 32 layer scaled 10.821 x 0.5, in a 343 x 193 scene, with the
# scroll effect repeating it 14 times across: Wallpaper Engine's repeats are about 0.9 % wider than owe's (116.5
# against 115.3 screen pixels at 1600 x 900), so the grass drifts apart towards the screen's edges. Here every column
# of a 32 x 32 texture has its own colour (red 8 x column + 4), so the picture says which texture column each screen
# pixel shows. Rows, bottom up: the grass's layer with the repeat (14), the same layer repeating once, the same
# layer without the effect, a layer scaled 5 x 0.5 repeating 14 times, and one scaled 1 x 0.5 repeating twice. The
# effect is scroll's sampling without the movement: frac(v_TexCoord * repeat).
import struct

FRAG_REPEAT = """varying vec4 v_TexCoord;
uniform sampler2D g_Texture0; // {"hidden":true}
uniform vec2 g_Scale; // {"material":"repeat","default":"1 1"}
void main() {
	gl_FragColor = texSample2D(g_Texture0, frac(v_TexCoord.xy * g_Scale));
}
"""
SIZE = 32


def column_texture():
    pixels = bytearray()
    for y in range(SIZE):
        for x in range(SIZE):
            pixels += bytes((8 * x + 4, 255 if y < SIZE // 2 else 128, (x % 2) * 255, 255))
    # flags 3: no interpolation, clamped coordinates (the grass's .tex-json: nointerpolation, clampuvs)
    data = b'TEXV0005\0TEXI0001\0' + struct.pack('<7I', binary.RGBA8888, 3, SIZE, SIZE, SIZE, SIZE, 0)
    data += b'TEXB0003\0' + struct.pack('<Ii', 1, -1) + struct.pack('<I', 1)
    data += struct.pack('<IIII', SIZE, SIZE, 0, len(pixels)) + struct.pack('<I', len(pixels)) + bytes(pixels)
    return data


w = Wallpaper('effect-repeat-scaled', 'an effect repeating its texture on a layer scaled unevenly',
              clear='0.1 0.1 0.15', size=(343, 193))
write_binary(os.path.join(w.name, 'materials', 'columns.tex'), column_texture())
model = w.texture_model('columns', 'columns', model_extra={"autosize": True})
local_effect(w, 'repeat', FRAG_REPEAT, {})
ROWS = (('grass, repeat 14', '10.821 0.5 1', '14 1'),
        ('grass, repeat 1', '10.821 0.5 1', '1 1'),
        ('grass, no effect', '10.821 0.5 1', None),
        ('scale 5, repeat 14', '5 0.5 1', '14 1'),
        ('scale 1, repeat 2', '1 0.5 1', '2 1'))
for k, (label, scale, repeat) in enumerate(ROWS):
    effects = [use('repeat', 10 + 10 * k, {"repeat": repeat})] if repeat else []
    # above the probe's records (the bottom fifth of the screen)
    w.image(label, model, 171.5, 60 + 25 * k, SIZE, SIZE, scale=scale, effects=effects)
w.save()
with open(os.path.join(HERE, w.name, 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, w.name))

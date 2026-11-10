# effect-target-matrix: what g_ModelViewProjectionMatrix and a_Position are in an effect pass drawn into a target of
# its own (effect-fbo-scale: a first pass placing its quad with the matrix leaves the layer out in Wallpaper Engine,
# while shipped passes into targets use vec4(a_Position, 1.0)). Three 128 x 128 layers, unscaled, in a 640 x 360
# scene, each an effect writing what it sees as colours:
# - "target": a first pass, into _rt_Work, writes the matrix as a 4 x 4 grid (cell column j, row i: entry
#   dot(mul(e_i, M), e_j)), a second copies it onto the layer;
# - "position": the same with a_Position (red 0.5 + x / 256, green 0.5 + y / 256, blue 0.5 + x / 4);
# - "final": the grid written by a one-pass effect, drawn onto the scene (the matrix of a last pass);
# - "chained": "target" followed by a one-pass copy effect, so its passes aren't the layer's last;
# - "compose" (above the others): the grid written by a "compose" first pass (as refraction's), copied by the second
#   (its g_Texture0, as refraction's second pass reads it).
# Each entry m is red 0.5 + m / 2, green 0.5 + m / 20, blue 0.5 + m / 200.
import struct

SIZE = 32
VERT_GRID = """uniform mat4 g_ModelViewProjectionMatrix;
attribute vec3 a_Position;
attribute vec2 a_TexCoord;
varying vec4 v_TexCoord;
varying vec4 v_Row0;
varying vec4 v_Row1;
varying vec4 v_Row2;
varying vec4 v_Row3;

void main() {
	gl_Position = %s;
	v_TexCoord = a_TexCoord.xyxy;
	v_Row0 = mul(vec4(1.0, 0.0, 0.0, 0.0), g_ModelViewProjectionMatrix);
	v_Row1 = mul(vec4(0.0, 1.0, 0.0, 0.0), g_ModelViewProjectionMatrix);
	v_Row2 = mul(vec4(0.0, 0.0, 1.0, 0.0), g_ModelViewProjectionMatrix);
	v_Row3 = mul(vec4(0.0, 0.0, 0.0, 1.0), g_ModelViewProjectionMatrix);
}
"""
FRAG_GRID = """varying vec4 v_TexCoord;
varying vec4 v_Row0;
varying vec4 v_Row1;
varying vec4 v_Row2;
varying vec4 v_Row3;
uniform sampler2D g_Texture0; // {"hidden":true}
void main() {
	vec2 cell = floor(saturate(v_TexCoord.xy) * 3.999);
	vec4 pick = vec4(1.0, 1.0, 1.0, 1.0) - min(abs(vec4(cell.y, cell.y, cell.y, cell.y) - vec4(0.0, 1.0, 2.0, 3.0)), vec4(1.0, 1.0, 1.0, 1.0));
	vec4 row = v_Row0 * pick.x + v_Row1 * pick.y + v_Row2 * pick.z + v_Row3 * pick.w;
	vec4 col = vec4(1.0, 1.0, 1.0, 1.0) - min(abs(vec4(cell.x, cell.x, cell.x, cell.x) - vec4(0.0, 1.0, 2.0, 3.0)), vec4(1.0, 1.0, 1.0, 1.0));
	float m = dot(row, col);
	gl_FragColor = vec4(saturate(0.5 + m / 2.0), saturate(0.5 + m / 20.0), saturate(0.5 + m / 200.0), 1.0) + texSample2D(g_Texture0, v_TexCoord.xy) * 0.0;
}
"""
VERT_POS = """attribute vec3 a_Position;
attribute vec2 a_TexCoord;
varying vec4 v_TexCoord;
varying vec4 v_Pos;

void main() {
	gl_Position = vec4(a_Position, 1.0);
	v_TexCoord = a_TexCoord.xyxy;
	v_Pos = vec4(a_Position, 1.0);
}
"""
FRAG_POS = """varying vec4 v_TexCoord;
varying vec4 v_Pos;
uniform sampler2D g_Texture0; // {"hidden":true}
void main() {
	gl_FragColor = vec4(saturate(0.5 + v_Pos.x / 256.0), saturate(0.5 + v_Pos.y / 256.0), saturate(0.5 + v_Pos.x / 4.0), 1.0) + texSample2D(g_Texture0, v_TexCoord.xy) * 0.0;
}
"""
FRAG_COPY = """varying vec4 v_TexCoord;
uniform sampler2D g_Texture0; // {"hidden":true}
void main() {
	gl_FragColor = texSample2D(g_Texture0, v_TexCoord.xy);
}
"""
MVP = 'mul(vec4(a_Position, 1.0), g_ModelViewProjectionMatrix)'


def grey_texture():
    pixels = bytes((128, 128, 128, 255)) * (SIZE * SIZE)
    data = b'TEXV0005\0TEXI0001\0' + struct.pack('<7I', binary.RGBA8888, 3, SIZE, SIZE, SIZE, SIZE, 0)
    data += b'TEXB0003\0' + struct.pack('<Ii', 1, -1) + struct.pack('<I', 1)
    data += struct.pack('<IIII', SIZE, SIZE, 0, len(pixels)) + struct.pack('<I', len(pixels)) + pixels
    return data


def shader(w, name, vert, frag):
    w.files['materials/effects/%s.json' % name] = {"passes": [{
        "shader": "effects/" + name, "blending": "normal", "depthtest": "disabled", "depthwrite": "disabled",
        "cullmode": "nocull"}]}
    w.files['shaders/effects/%s.vert' % name] = vert
    w.files['shaders/effects/%s.frag' % name] = frag


w = Wallpaper('effect-target-matrix', 'the matrix and positions of an effect pass into a target',
              clear='0.1 0.1 0.15', size=(640, 360))
write_binary(os.path.join(w.name, 'materials', 'grey.tex'), grey_texture())
model = w.texture_model('grey', 'grey', model_extra={"autosize": True})
shader(w, 'gridtarget', VERT_GRID % 'vec4(a_Position, 1.0)', FRAG_GRID)
shader(w, 'gridfinal', VERT_GRID % MVP, FRAG_GRID)
shader(w, 'pos', VERT_POS, FRAG_POS)
shader(w, 'copy', VERT, FRAG_COPY)
fbos = [{"name": "_rt_Work", "format": "rgba8888", "scale": 1}]
for name, first in (('target', 'gridtarget'), ('position', 'pos')):
    w.files['effects/%s/effect.json' % name] = {"version": 1, "name": name, "fbos": fbos, "passes": [
        {"material": "materials/effects/%s.json" % first, "target": "_rt_Work",
         "bind": [{"name": "previous", "index": 0}]},
        {"material": "materials/effects/copy.json", "bind": [{"name": "_rt_Work", "index": 0}]}]}
w.files['effects/copy/effect.json'] = {"version": 1, "name": "copy",
                                       "passes": [{"material": "materials/effects/copy.json"}]}
w.files['effects/compose/effect.json'] = {"version": 1, "name": "compose", "passes": [
    {"material": "materials/effects/gridtarget.json", "compose": True},
    {"material": "materials/effects/copy.json"}]}
w.files['effects/final/effect.json'] = {"version": 1, "name": "final",
                                        "passes": [{"material": "materials/effects/gridfinal.json"}]}
for i, (name, x) in enumerate((('target', 80), ('position', 240), ('final', 400))):
    w.image(name, model, x, 180, 128, 128,
            effects=[{"file": "effects/%s/effect.json" % name, "id": 11 + i, "name": name, "visible": True,
                      "passes": [{"id": 100 + 10 * i + n} for n in range(2 if name != 'final' else 1)]}])
w.image('chained', model, 560, 180, 128, 128,
        effects=[{"file": "effects/target/effect.json", "id": 21, "name": "target", "visible": True,
                  "passes": [{"id": 200}, {"id": 201}]},
                 {"file": "effects/copy/effect.json", "id": 22, "name": "copy", "visible": True,
                  "passes": [{"id": 210}]}])
w.image('compose', model, 320, 100, 128, 128,
        effects=[{"file": "effects/compose/effect.json", "id": 31, "name": "compose", "visible": True,
                  "passes": [{"id": 300}, {"id": 301}]}])
w.save()
with open(os.path.join(HERE, w.name, 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, w.name))

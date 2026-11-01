# effect-fbo-wrap: an effect with a render target of its own: "tiles", a 4 x 4 pixel target that repeats (width 4,
# height 4, "uvs": "repeat", as glitter's tiles are 256 x 256 and repeat). Its first pass paints each tile pixel
# from its place (red by x, green by y), its second pass lays the tiles over the picture three times across. If the
# target kept the picture's size and clamped its edges the cell would show a smooth gradient streaked at its right
# and bottom; with the target's own size it shows 4 x 4 blocks, repeated 3 times across.
FRAG_TILES = """varying vec4 v_TexCoord;
uniform sampler2D g_Texture0; // {"hidden":true}
void main() {
	vec4 albedo = texSample2D(g_Texture0, v_TexCoord.xy);
	gl_FragColor = vec4(v_TexCoord.x, v_TexCoord.y, 0.0, 1.0) + albedo * 0.0;
}
"""
FRAG_LAY = """varying vec4 v_TexCoord;
uniform sampler2D g_Texture0; // {"hidden":true}
uniform sampler2D g_Texture1; // {"hidden":true}
void main() {
	vec4 albedo = texSample2D(g_Texture0, v_TexCoord.xy);
	vec3 tile = texSample2D(g_Texture1, v_TexCoord.xy * 3.0).rgb;
	gl_FragColor = vec4(albedo.rgb * 0.25 + tile * 0.75, albedo.a);
}
"""
w = Wallpaper('effect-fbo-wrap', 'an effect render target with its own size and repeating coordinates',
              clear='0.1 0.1 0.2')
# the paint pass fills its whole target (a vertex shader without the layer's matrix, as glitter's)
VERT_FULL = """attribute vec3 a_Position;
attribute vec2 a_TexCoord;
varying vec4 v_TexCoord;

void main() {
	gl_Position = vec4(a_Position, 1.0);
	v_TexCoord = a_TexCoord.xyxy;
}
"""
for name in ('tiles_paint', 'tiles_lay'):
    w.files['materials/effects/%s.json' % name] = {"passes": [{
        "shader": "effects/" + name, "blending": "normal", "depthtest": "disabled", "depthwrite": "disabled",
        "cullmode": "nocull"}]}
    w.files['shaders/effects/%s.vert' % name] = VERT_FULL if name == 'tiles_paint' else VERT
w.files['shaders/effects/tiles_paint.frag'] = FRAG_TILES
w.files['shaders/effects/tiles_lay.frag'] = FRAG_LAY
# variants of the target: its own 4 x 4 size repeating; 4 x 4 clamped; the picture's size repeating; the picture's
# size clamped (what owe did)
VARIANTS = (('own size, repeat', {"width": 4, "height": 4, "uvs": "repeat"}),
            ('own size, clamp', {"width": 4, "height": 4}),
            ('picture size, repeat', {"uvs": "repeat"}),
            ('picture size, clamp', {}))
model = w.texture_model('picture', 'util/clouds_256')
for k, (label, fbo) in enumerate(VARIANTS):
    name = 'tiles%d' % k
    w.files['effects/%s/effect.json' % name] = {
        "version": 1, "name": name,
        "passes": [{"material": "materials/effects/tiles_paint.json", "target": "_rt_Tiles"},
                   {"material": "materials/effects/tiles_lay.json",
                    "bind": [{"name": "previous", "index": 0}, {"name": "_rt_Tiles", "index": 1}]}],
        "fbos": [dict({"name": "_rt_Tiles", "format": "rgba8888"}, **fbo)]}
    w.image(label, model, 330 + 420 * k, 640, 256, 256, scale='1.1 1.1 1', effects=[
        {"file": "effects/%s/effect.json" % name, "id": 10 + k * 10, "name": name, "visible": True,
         "passes": [{"id": 101 + k * 10}, {"id": 102 + k * 10}]}])
w.image('plain', model, 960, 250, 256, 256, scale='1.1 1.1 1')
w.save()
with open(os.path.join(HERE, 'effect-fbo-wrap', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'effect-fbo-wrap'))

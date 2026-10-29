# 19g. model-in-2d: a model in a 2D scene (Wallpaper Engine's "retro" draws its background so), its shader placing it
# in clip space, behind a layer
w = Wallpaper('model-in-2d', 'a model in a 2D scene', clear='0 0 0')
w.files['shaders/clipspace.vert'] = """attribute vec3 a_Position;
attribute vec2 a_TexCoord;
varying vec2 v_TexCoord;

void main() {
	gl_Position = vec4(a_TexCoord * 2 - 1, 0.5, 1);
	v_TexCoord = a_TexCoord;
}
"""
w.files['shaders/clipspace.frag'] = """varying vec2 v_TexCoord;

void main() {
	gl_FragColor = vec4(1.0 - v_TexCoord.y, v_TexCoord.x * 0.5, v_TexCoord.y, 1.0);
}
"""
w.files['materials/clipspace.json'] = {"passes": [{
    "shader": "clipspace", "blending": "normal", "cullmode": "nocull", "depthtest": "disabled",
    "depthwrite": "disabled", "textures": [None]}]}
write_binary(os.path.join(w.name, 'models/clipspace.mdl'), cube_mdl('materials/clipspace.json'))
w.add({"name": "background", "model": "models/clipspace.mdl", "origin": "0 0 0", "angles": "0 0 0", "scale": "1 1 1"})
w.solid('square', 1400, 700, 300, 300, '1 1 1')
w.save()
# 19h. model-in-2d-wide: the same in a scene wider than the screen. The real engine draws only the part the screen
# shows, so the shader's clip space (its colours from left to right) spans the screen, not the whole scene
clipspace_files = w.files
w = Wallpaper('model-in-2d-wide', 'clip space in a scene wider than the screen', clear='0 0 0', size=(2520, 1080))
w.files = dict(clipspace_files)
write_binary(os.path.join(w.name, 'models/clipspace.mdl'), cube_mdl('materials/clipspace.json'))
w.add({"name": "background", "model": "models/clipspace.mdl", "origin": "300 0 0", "angles": "0 0 0", "scale": "1 1 1"})
w.solid('square', 1700, 700, 300, 300, '1 1 1')
w.save()

# 19e. model-clipspace: a model whose shader places it straight in clip space from its texture coordinates (DNA
# Fragment's background does this), coloured by them: red where v is 0, blue where it is 1; a normal cube in front
w = Scene3D('model-clipspace', 'a model drawn in clip space by its shader', ambient='1 1 1')
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
w.cube('background', 'models/clipspace.mdl', 0, 0, 0)
w.cube('cube', w.material('white', '1 1 1'), 0, 1.5, 0, (0.3, 0.5, 0), '1.5 1.5 1.5')
w.save()

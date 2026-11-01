# reflection-what: what Wallpaper Engine's planar reflection draws. A floor whose shader shows _rt_Reflection at its screen
# position, a red cube marked "reflected", a green cube not marked, a blue 2D image layer, and the clock probe (a fullscreen
# layer with an effect). Arsenal's floor reflects the probe's record strip in Wallpaper Engine; owe reflects only "reflected"
# objects.
w = Scene3D('reflection-what', 'what the planar reflection draws', eye='0 2 12')
red = w.material('red', '1 0.2 0.2')
green = w.material('green', '0.2 1 0.2')
w.files['materials/floor.json'] = {"passes": [{
    "shader": "reflectfloor", "blending": "normal", "cullmode": "nocull", "depthtest": "enabled", "depthwrite": "enabled",
    "textures": ["_rt_Reflection"]}]}
floor = 'models/floor.mdl'
write_binary(os.path.join(w.name, floor), cube_mdl('materials/floor.json'))
write(w.name + '/shaders/reflectfloor.vert', """
uniform mat4 g_ModelViewProjectionMatrix;
attribute vec3 a_Position;
varying vec3 v_ScreenPos;
void main() {
	gl_Position = mul(vec4(a_Position, 1.0), g_ModelViewProjectionMatrix);
	v_ScreenPos = gl_Position.xyw;
}
""")
write(w.name + '/shaders/reflectfloor.frag', """
uniform sampler2D g_Texture0;
varying vec3 v_ScreenPos;
void main() {
	vec2 uv = (v_ScreenPos.xy / v_ScreenPos.z) * 0.5 + 0.5;
	gl_FragColor = vec4(texSample2D(g_Texture0, uv).rgb * 0.8 + vec3(0.1, 0.1, 0.12), 1.0);
}
""")
w.add({"name": "floor", "model": floor, "origin": "0 -1.6 0", "angles": "0 0 0", "scale": "7 0.05 4"})
w.add({"name": "reflected cube", "model": red, "origin": "-2.5 0 0", "angles": "0 0.5 0", "scale": "1 1 1", "reflected": True})
w.add({"name": "unreflected cube", "model": green, "origin": "2.5 0 0", "angles": "0 0.5 0", "scale": "1 1 1"})
w.add({"name": "2d layer", "image": "models/util/solidlayer.json", "origin": "960 880 0", "size": "300 120",
       "color": "0.2 0.4 1", "alpha": 1, "angles": "0 0 0", "scale": "1 1 1", "alignment": "center"})
w.save()
with open(os.path.join(HERE, 'reflection-what', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'reflection-what'))

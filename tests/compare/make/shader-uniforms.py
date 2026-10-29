# 11. shader-uniforms: effects written for the test that read different kinds of constants and built-in uniforms
w = Wallpaper('shader-uniforms', 'shader constants and built-in uniforms', clear='0.15 0.15 0.15')
rainbow = w.texture_model('rainbow', 'gradient/gradient_rainbow')
fire = w.texture_model('fire', 'gradient/gradient_fire')
local_effect(w, 'probe', """varying vec4 v_TexCoord;
uniform sampler2D g_Texture0; // {"hidden":true}
uniform vec4 g_Texture0Resolution;
uniform float u_float; // {"material":"float","label":"float","default":0.5,"range":[0,1]}
uniform vec2 u_vec2; // {"material":"vec2","label":"vec2","default":"0.2 0.8"}
uniform vec3 u_vec3; // {"material":"vec3","label":"vec3","default":"1 0 0","type":"color"}
uniform vec4 u_vec4; // {"material":"vec4","label":"vec4","default":"0 0 1 0.5"}

void main() {
	vec4 albedo = texSample2D(g_Texture0, v_TexCoord.xy);
#if MODE == 1
	// the constants as colours: left to right float, vec2, vec3, vec4
	if (v_TexCoord.x < 0.25) {
		albedo.rgb = vec3(u_float, u_float, u_float);
	} else if (v_TexCoord.x < 0.5) {
		albedo.rgb = vec3(u_vec2.x, u_vec2.y, 0.0);
	} else if (v_TexCoord.x < 0.75) {
		albedo.rgb = u_vec3;
	} else {
		albedo.rgb = u_vec4.rgb * u_vec4.a;
	}
#endif
#if MODE == 2
	// the texture's size: its width in red (in 1/100), its height in green (in 1/10), the width of its content in blue
	albedo.rgb = vec3(g_Texture0Resolution.x / 100.0, g_Texture0Resolution.y / 10.0, g_Texture0Resolution.z / 100.0);
#endif
#if MODE == 3
	// the texture coordinates
	albedo.rgb = vec3(v_TexCoord.x, v_TexCoord.y, 0.0);
#endif
	gl_FragColor = albedo;
}
""", None)
# a combo (MODE) chooses the code that runs
w.files['materials/effects/probe.json'] = {"passes": [{
    "shader": "effects/probe", "blending": "normal", "depthtest": "disabled", "depthwrite": "disabled",
    "cullmode": "nocull"}]}
for i, mode in enumerate([0, 1, 2, 3]):
    effect_use = {"file": "effects/probe/effect.json", "id": 10 + i, "name": "probe", "visible": True,
                  "passes": [{"id": 110 + i, "combos": {"MODE": mode}, "constantshadervalues": {
                      "float": 0.25, "vec2": "0.5 0.25", "vec3": "0 1 0", "vec4": "1 0 1 0.5"}}]}
    w.image('probe %d' % mode, rainbow, 250 + i * 450, 800, 320, 200, effects=[effect_use])
    effect_use2 = json.loads(json.dumps(effect_use))
    effect_use2['id'] = 20 + i
    w.image('probe fire %d' % mode, fire, 250 + i * 450, 450, 320, 200, effects=[effect_use2])
w.save()

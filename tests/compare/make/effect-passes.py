# 12. effect-passes: an effect with several passes and an intermediate buffer (a box blur done in two passes); the
# effect.json follows the ones that ship with Wallpaper Engine (the first pass has a target, the last one binds it)
w = Wallpaper('effect-passes', 'multi-pass effects', clear='0.1 0.1 0.1')
rainbow = w.texture_model('rainbow', 'gradient/gradient_rainbow')
w.files['effects/boxblur/effect.json'] = {
    "version": 1, "name": "boxblur",
    "passes": [
        {"material": "materials/effects/boxblur_x.json", "target": "_rt_BlurTemp"},
        {"material": "materials/effects/boxblur_y.json", "bind": [{"name": "_rt_BlurTemp", "index": 0}]}],
    "fbos": [{"name": "_rt_BlurTemp", "format": "rgba_backbuffer"}]}
for axis in 'xy':
    w.files['materials/effects/boxblur_%s.json' % axis] = {"passes": [{
        "shader": "effects/boxblur", "blending": "normal", "depthtest": "disabled", "depthwrite": "disabled",
        "cullmode": "nocull", "combos": {"VERTICAL": 1 if axis == 'y' else 0}}]}
# a pass that draws into a buffer fills it (clip space); only the last one goes through the layer's transform
w.files['shaders/effects/boxblur.vert'] = VERT.replace(
    '\tgl_Position = mul(vec4(a_Position, 1.0), g_ModelViewProjectionMatrix);',
    '#if VERTICAL\n\tgl_Position = mul(vec4(a_Position, 1.0), g_ModelViewProjectionMatrix);\n#else\n'
    '\tgl_Position = vec4(a_Position, 1.0);\n#endif')
w.files['shaders/effects/boxblur.frag'] = """varying vec4 v_TexCoord;
uniform sampler2D g_Texture0; // {"hidden":true}
uniform vec4 g_Texture0Resolution;
uniform float u_radius; // {"material":"radius","label":"Radius","default":2,"range":[0,10]}

void main() {
#if VERTICAL
	vec2 offset = vec2(0.0, 1.0 / g_Texture0Resolution.y);
#else
	vec2 offset = vec2(1.0 / g_Texture0Resolution.x, 0.0);
#endif
	// (a loop with a texture sample in it does not compile in the real Wallpaper Engine)
	vec4 sum = texSample2D(g_Texture0, v_TexCoord.xy - offset * 2.0 * u_radius)
		+ texSample2D(g_Texture0, v_TexCoord.xy - offset * u_radius)
		+ texSample2D(g_Texture0, v_TexCoord.xy)
		+ texSample2D(g_Texture0, v_TexCoord.xy + offset * u_radius)
		+ texSample2D(g_Texture0, v_TexCoord.xy + offset * 2.0 * u_radius);
	gl_FragColor = sum / 5.0;
}
"""
# a hard-edged image: the blur softens its edges
w.solid('bar', 400, 700, 20, 300, '1 1 1')
w.image('blurred rainbow', rainbow, 1100, 700, 64, 2, scale='8 100 1', effects=[
    {"file": "effects/boxblur/effect.json", "id": 10, "name": "boxblur", "visible": True,
     "passes": [{"id": 110, "constantshadervalues": {"radius": 3}}, {"id": 111, "constantshadervalues": {"radius": 3}}]}])
w.image('sharp rainbow', rainbow, 1100, 400, 64, 2, scale='8 100 1')
w.save()

# 4. effect-mix: effects written for this test (a colour mix and an opacity scale), so the wallpaper needs nothing
# but the shader pipeline: constants from the scene, several effects on a layer, and effects in a chain
w = Wallpaper('effect-mix', 'effects', clear='0.15 0.15 0.15')
rainbow = w.texture_model('rainbow', 'gradient/gradient_rainbow')
local_effect(w, 'colormix', """varying vec4 v_TexCoord;
uniform sampler2D g_Texture0; // {"hidden":true}
uniform vec3 u_color; // {"material":"color","label":"Colour","type":"color","default":"1 0 0"}
uniform float u_amount; // {"material":"amount","label":"Amount","default":0.5,"range":[0,1]}

void main() {
	vec4 albedo = texSample2D(g_Texture0, v_TexCoord.xy);
	albedo.rgb = mix(albedo.rgb, u_color, u_amount);
	gl_FragColor = albedo;
}
""", None)
local_effect(w, 'alphascale', """varying vec4 v_TexCoord;
uniform sampler2D g_Texture0; // {"hidden":true}
uniform float u_scale; // {"material":"scale","label":"Opacity","default":1,"range":[0,1]}

void main() {
	vec4 albedo = texSample2D(g_Texture0, v_TexCoord.xy);
	albedo.a *= u_scale;
	gl_FragColor = albedo;
}
""", None)
for i, amount in enumerate([0.0, 0.25, 0.5, 0.75, 1.0]):
    w.image('mix red %g' % amount, rainbow, 200 + i * 360, 860, 64, 2, scale='5 100 1',
            effects=[use('colormix', 10 + i, {"color": "1 0 0", "amount": amount})])
for i, color in enumerate(['0 0 1', '0 1 0', '1 1 1', '0 0 0']):
    w.image('mix full %d' % i, rainbow, 200 + i * 360, 600, 64, 2, scale='5 100 1',
            effects=[use('colormix', 20 + i, {"color": color, "amount": 1.0})])
for i, amount in enumerate([1.0, 0.5, 0.25]):
    w.solid('opacity back %d' % i, 300 + i * 500, 300, 400, 260, '1 1 1')
    w.image('opacity %g' % amount, rainbow, 300 + i * 500, 300, 64, 2, scale='4.6875 100 1',
            effects=[use('alphascale', 30 + i, {"scale": amount})])
# two effects on one layer run one after the other: halve the opacity, then mix half way to blue
w.image('chain', rainbow, 1700, 300, 64, 2, scale='4.375 100 1', effects=[
    use('alphascale', 40, {"scale": 0.5}), use('colormix', 41, {"color": "0 0 1", "amount": 0.5})])
w.save()

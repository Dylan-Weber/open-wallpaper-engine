# 13. effect-blending: the blending mode a pass draws with
w = Wallpaper('effect-blending', 'blending of effect passes', clear='0.3 0.3 0.3')
rainbow = w.texture_model('rainbow', 'gradient/gradient_rainbow')
for i, blending in enumerate(['normal', 'translucent', 'additive']):
    w.files['effects/blend_%s/effect.json' % blending] = {
        "version": 1, "name": blending, "passes": [{"material": "materials/effects/blend_%s.json" % blending}]}
    w.files['materials/effects/blend_%s.json' % blending] = {"passes": [{
        "shader": "effects/halfalpha", "blending": blending, "depthtest": "disabled", "depthwrite": "disabled",
        "cullmode": "nocull"}]}
w.files['shaders/effects/halfalpha.vert'] = VERT
w.files['shaders/effects/halfalpha.frag'] = """varying vec4 v_TexCoord;
uniform sampler2D g_Texture0; // {"hidden":true}

void main() {
	vec4 albedo = texSample2D(g_Texture0, v_TexCoord.xy);
	albedo.a *= 0.5;
	gl_FragColor = albedo;
}
"""
for i, blending in enumerate(['normal', 'translucent', 'additive']):
    w.solid('back %s' % blending, 400 + i * 550, 700, 450, 300, '0.2 0.4 0.8')
    w.image('layer %s' % blending, rainbow, 400 + i * 550, 700, 64, 2, scale='6 100 1', effects=[
        {"file": "effects/blend_%s/effect.json" % blending, "id": 10 + i, "name": blending, "visible": True,
         "passes": [{"id": 110 + i}]}])
w.save()

# effect-hidden-combo: a combo that is hidden by its "require" and left at its default 0, but which the shader reads as
# a value (float(SHARP), as More Physics' Gradient Generator does). Wallpaper Engine compiles it as 0; if owe leaves it
# undefined the effect fails to build and the layer is drawn plain. Left cell: the picture; middle: the effect with
# MODE 0 (SHARP hidden, so the picture is tinted by 0.5 + 0.5 * 0); right: MODE 4 and SHARP 1 (the tint is 1).
w = Wallpaper('effect-hidden-combo', 'a hidden combo read as a value', clear='0.12 0.12 0.12')
local_effect(w, 'hiddencombo', """// [COMBO] {"material":"Mode","combo":"MODE","type":"options","default":0,"options":{"a":0,"b":4}}
// [COMBO] {"material":"Sharp","combo":"SHARP","type":"options","default":0,"require":{"MODE":4}}
varying vec4 v_TexCoord;
uniform sampler2D g_Texture0; // {"hidden":true}
void main() {
	vec4 albedo = texSample2D(g_Texture0, v_TexCoord.xy);
	float k = 0.5 + 0.5 * float(SHARP);
	gl_FragColor = vec4(albedo.rgb * k, albedo.a);
}
""", {})
model = w.texture_model('picture', 'util/clouds_256')


def effect(id, combos):
    e = use('hiddencombo', id, {})
    e['passes'][0]['combos'] = combos
    return e


for i, (label, effects) in enumerate([('plain', []), ('hidden', [effect(10, {"MODE": 0})]),
                                       ('shown', [effect(20, {"MODE": 4, "SHARP": 1})])]):
    w.image(label, model, 400 + i * 560, 880, 256, 256, scale='1.8 1.8 1', effects=effects)
w.save()
with open(os.path.join(HERE, 'effect-hidden-combo', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'effect-hidden-combo'))

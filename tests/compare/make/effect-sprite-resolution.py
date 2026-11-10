# effect-sprite-resolution: g_Texture0Resolution of an effect's first pass on a sprite layer. Wallpaper Engine gives the
# frame's size in all four components (a plain texture: its size padded to a multiple of 4 in all four); owe gave the
# whole sheet. Each layer wears an effect that paints the four components in two grey bands each (v / 64 in 1/128
# steps, then the rest of v / 64), x to w from left to right. Rows: a sprite sheet of 5 frames, a sprite that is a
# strip, one with frames of 224x64, and a plain gradient.
FRAG = """varying vec4 v_TexCoord;
uniform sampler2D g_Texture0; // {"hidden":true}
uniform vec4 g_Texture0Resolution;

float hi(float v) { return floor(v / 64.0) / 128.0; }
float lo(float v) { return frac(v / 64.0); }

void main() {
	float c = floor(v_TexCoord.x * 8.0);
	float v = 0.0;
	if (c < 2.0) v = g_Texture0Resolution.x;
	else if (c < 4.0) v = g_Texture0Resolution.y;
	else if (c < 6.0) v = g_Texture0Resolution.z;
	else v = g_Texture0Resolution.w;
	float g = (c == 0.0 || c == 2.0 || c == 4.0 || c == 6.0) ? hi(v) : lo(v);
	gl_FragColor = vec4(g, g, g, 1.0);
}
"""
w = Wallpaper('effect-sprite-resolution', "g_Texture0Resolution of an effect's first pass on a sprite layer", clear='0 0 0')
local_effect(w, 'res', FRAG, {})
w.files['materials/effects/res.json']["passes"][0].update({"depthtest": "disabled", "depthwrite": "disabled"})
for i, (name, texture) in enumerate((('petals', 'particle/nature/rosepetals'), ('splash', 'particle/water/splash_10'),
                                     ('ripple', 'particle/water/ripple_single'),
                                     ('gradient', 'gradient/gradient_rainbow'))):
    model = w.texture_model(name, texture)
    w.image(name, model, 960, 900 - i * 220, 1200, 160, effects=[use('res', 10 + i, {})])
w.save()
with open(os.path.join(HERE, 'effect-sprite-resolution', 'compare.json'), 'w') as f:
    json.dump({"probe": True, "fps": 30}, f)

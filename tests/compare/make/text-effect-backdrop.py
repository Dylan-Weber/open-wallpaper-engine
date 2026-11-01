# text-effect-backdrop: text layers with effects over a picture: a blur, and an effect tinting its whole frame,
# left- and centre-aligned. Shows whether an effect works on the text alone or on the scene behind it too.
import shutil
w = Wallpaper('text-effect-backdrop', 'effects on text over a picture', clear='0.1 0.12 0.18')
os.makedirs(os.path.join(HERE, 'text-effect-backdrop', 'fonts'), exist_ok=True)
shutil.copy('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',
            os.path.join(HERE, 'text-effect-backdrop', 'fonts', 'DejaVuSans.ttf'))
local_effect(w, 'frame', """varying vec4 v_TexCoord;
uniform sampler2D g_Texture0; // {"hidden":true}
void main() {
	vec4 albedo = texSample2D(g_Texture0, v_TexCoord.xy);
	gl_FragColor = albedo + (1.0 - albedo.a) * vec4(0.35, 0.08, 0.08, 0.6);
}
""", {})
model = w.texture_model('picture', 'util/clouds_256')
w.image('backdrop', model, 960, 540, 256, 256, scale='6 3.4 1')
cells = (("blur", "center", shipped('blur', id=10)), ("frame", "center", use('frame', 20, {})),
         ("blur", "left", shipped('blur', id=30)), ("frame", "left", use('frame', 40, {})))
for k, (name, h, effect) in enumerate(cells):
    x, y = 480 + 960 * (k % 2) - (200 if h == 'left' else 0), 800 - 500 * (k // 2)
    w.add({"name": "t%d" % k, "text": "Text %s" % name, "font": "fonts/DejaVuSans.ttf", "pointsize": 40,
           "size": "500 150", "origin": "%d %d 0" % (x, y), "angles": "0 0 0", "scale": "1 1 1", "alpha": 1,
           "color": "1 0.85 0.3", "horizontalalign": h, "verticalalign": "center", "padding": 0, "visible": True,
           "opaquebackground": False, "effects": [effect]})
w.save()
copy_shipped_effects(w, ['blur'])
with open(os.path.join(HERE, 'text-effect-backdrop', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'text-effect-backdrop'))

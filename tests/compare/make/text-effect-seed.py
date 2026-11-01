# text-effect-seed: text with an effect starts from a copy of the scene behind it (alpha 0), which an effect tinting
# its whole frame shows. Plain words in the upper half, framed words at the mirrored places in the lower half: a copy
# taken upside down shows the upper words inside the lower frames.
import shutil
w = Wallpaper('text-effect-seed', 'the scene copy behind text with an effect', clear='0.1 0.12 0.18')
os.makedirs(os.path.join(HERE, 'text-effect-seed', 'fonts'), exist_ok=True)
shutil.copy('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',
            os.path.join(HERE, 'text-effect-seed', 'fonts', 'DejaVuSans.ttf'))
local_effect(w, 'frame', """varying vec4 v_TexCoord;
uniform sampler2D g_Texture0; // {"hidden":true}
void main() {
	vec4 albedo = texSample2D(g_Texture0, v_TexCoord.xy);
	gl_FragColor = albedo + (1.0 - albedo.a) * vec4(0.35, 0.08, 0.08, 0.6);
}
""", {})
for k, x in enumerate((480, 1440)):
    for row, (y, effect) in enumerate(((760, False), (320, True))):
        w.add({"name": "w%d%d" % (k, row), "text": "Text above" if not effect else "ab", "font": "fonts/DejaVuSans.ttf",
               "pointsize": 40 if not effect else 90, "size": "800 220", "origin": "%d %d 0" % (x, y),
               "angles": "0 0 0", "scale": "1 1 1", "alpha": 1, "color": "0.6 0.9 1" if not effect else "1 0.85 0.3",
               "horizontalalign": "center", "verticalalign": "center", "padding": 0, "visible": True,
               "opaquebackground": False, "effects": [use('frame', 10 + k, {})] if effect else []})
w.save()
with open(os.path.join(HERE, 'text-effect-seed', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'text-effect-seed'))

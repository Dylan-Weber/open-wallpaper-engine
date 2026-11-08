# text-effect-frame: an effect that tints its whole frame shows the size of a text layer's effect input against the
# layer's authored size: text wider than its size, text in a size larger than it with each alignment, and padding.
import shutil
w = Wallpaper('text-effect-frame', 'the frame of an effect on a text layer', clear='0.1 0.12 0.18')
os.makedirs(os.path.join(HERE, 'text-effect-frame', 'fonts'), exist_ok=True)
shutil.copy('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',
            os.path.join(HERE, 'text-effect-frame', 'fonts', 'DejaVuSans.ttf'))
local_effect(w, 'frame', """varying vec4 v_TexCoord;
uniform sampler2D g_Texture0; // {"hidden":true}
void main() {
	vec4 albedo = texSample2D(g_Texture0, v_TexCoord.xy);
	gl_FragColor = albedo + (1.0 - albedo.a) * vec4(0.35, 0.08, 0.08, 0.6);
}
""", {})
cells = (("Wide text over a small size", "300 80", "center", "center", 0),
         ("Short", "700 220", "center", "center", 0),
         ("Short", "700 220", "left", "top", 0),
         ("Short", "700 220", "right", "bottom", 0),
         ("Padded", "300 80", "center", "center", 30),
         ("Two\nlines", "200 60", "left", "center", 0))
for k, (text, size, h, v, pad) in enumerate(cells):
    x, y = 480 + 960 * (k % 2), 900 - 330 * (k // 2)
    w.add({"name": "t%d" % k, "text": text, "font": "fonts/DejaVuSans.ttf", "pointsize": 32, "size": size,
           "origin": "%d %d 0" % (x, y), "angles": "0 0 0", "scale": "1 1 1", "alpha": 1, "color": "1 0.85 0.3",
           "horizontalalign": h, "verticalalign": v, "padding": pad, "visible": True, "opaquebackground": False,
           "effects": [use('frame', 10 + k, {})]})
w.save()
with open(os.path.join(HERE, 'text-effect-frame', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'text-effect-frame'))

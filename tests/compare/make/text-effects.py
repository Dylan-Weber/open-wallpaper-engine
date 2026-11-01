# text-effects: effects on text layers (DejaVu Sans shipped with the wallpaper): none, tint, blur, opacity and skew, so
# the effect's input (the text's render target, its size and padding) shows.
import shutil
w = Wallpaper('text-effects', 'effects on text layers', clear='0.1 0.12 0.18')
os.makedirs(os.path.join(HERE, 'text-effects', 'fonts'), exist_ok=True)
shutil.copy('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', os.path.join(HERE, 'text-effects', 'fonts', 'DejaVuSans.ttf'))
cells = (("none", None), ("tint", shipped('tint', {"color": "0.2 1 0.4"}, id=10)), ("blur", shipped('blur', id=20)),
         ("opacity", shipped('opacity', {"alpha": 0.4}, id=30)), ("skew", shipped('skew', id=40)))
for k, (name, effect) in enumerate(cells):
    x, y = 480 + 960 * (k % 2), 880 - 300 * (k // 2)
    w.add({"name": name, "text": "Text %s" % name, "font": "fonts/DejaVuSans.ttf", "pointsize": 40, "size": "700 200",
           "origin": "%d %d 0" % (x, y), "angles": "0 0 0", "scale": "1 1 1", "alpha": 1, "color": "1 0.85 0.3",
           "horizontalalign": "center", "verticalalign": "center", "padding": "0 0", "alignment": "center",
           "visible": True, "opaquebackground": False, "effects": [effect] if effect else []})
w.save()
copy_shipped_effects(w, ['tint', 'blur', 'opacity', 'skew'])
with open(os.path.join(HERE, 'text-effects', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'text-effects'))

# text-outline-alpha: translucent text with an outline (alpha 1, 0.6 and 0.3, outline thickness 6) over solid stripes:
# Wallpaper Engine fades letters and outline together, so the outline doesn't show through the letters.
import shutil
w = Wallpaper('text-outline-alpha', 'translucent outlined text', clear='0.1 0.12 0.18')
os.makedirs(os.path.join(HERE, 'text-outline-alpha', 'fonts'), exist_ok=True)
shutil.copy('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',
            os.path.join(HERE, 'text-outline-alpha', 'fonts', 'DejaVuSans.ttf'))
for k in range(4):
    base_layer(w, 'stripe %d' % k, size='1920 90', origin='960 %d 0' % (285 + 180 * k), color='0.3 0.55 0.35')
for k, alpha in enumerate((1.0, 0.6, 0.3)):
    w.add({"name": "alpha %s" % alpha, "text": "Outline", "font": "fonts/DejaVuSans.ttf", "pointsize": 30,
           "size": "800 220", "origin": "%d 600 0" % (380 + 600 * k), "angles": "0 0 0", "scale": "1 1 1",
           "alpha": alpha, "color": "1 0.85 0.3", "horizontalalign": "center", "verticalalign": "center",
           "padding": 0, "alignment": "center", "visible": True, "opaquebackground": False,
           "outline": True, "outlinecolor": "0.6 0.05 0.05", "outlinethickness": 6})
w.save()
with open(os.path.join(HERE, 'text-outline-alpha', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'text-outline-alpha'))

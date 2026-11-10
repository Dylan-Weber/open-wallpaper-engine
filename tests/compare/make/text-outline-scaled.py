# text-outline-scaled: outlined text on a scaled layer (Ocarina of Time's clock: 7 points on layers scaled 4). Wallpaper
# Engine's distance-field outlines stay sharp at any scale; owe rasterised them in layer units and blurred them. 7-point
# text with a thickness 2 outline on layers scaled 4 (translucent, drawn through its composite, and opaque), and 28 points
# at scale 1 for comparison.
import shutil
w = Wallpaper('text-outline-scaled', 'outlined text on a scaled layer', clear='0.12 0.12 0.22')
os.makedirs(os.path.join(HERE, w.name, 'fonts'), exist_ok=True)
shutil.copy('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', os.path.join(HERE, w.name, 'fonts', 'DejaVuSans.ttf'))
for name, x, size, scale, alpha in (('7pt x4 translucent', 300, 7, 4, 0.34), ('7pt x4', 960, 7, 4, 1.0),
                                    ('28pt', 1600, 28, 1, 1.0)):
    w.add({"name": name, "text": "PM 12:00\nFriday", "font": "fonts/DejaVuSans.ttf", "pointsize": size,
           "size": "268 105", "origin": "%d 540 0" % x, "angles": "0 0 0", "scale": "%d %d 1" % (scale, scale),
           "alpha": alpha, "color": "0.86 0 0", "horizontalalign": "center", "verticalalign": "center",
           "padding": "0 0", "alignment": "center", "visible": True, "opaquebackground": False, "outline": True,
           "outlinecolor": "0.34 0 0", "outlinethickness": 2.0})
w.save()
with open(os.path.join(HERE, w.name, 'compare.json'), 'w') as f:
    json.dump({"probe": True, "fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, w.name))

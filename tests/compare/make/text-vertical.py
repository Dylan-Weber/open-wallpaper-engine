# text-vertical: where a line of text sits vertically in its box, by point size, with a font shipped with the wallpaper
# (DejaVu Sans): in text-outline owe's letters sat about 0.065 x the point size lower than Wallpaper Engine's. "H" and
# "Hg" at 20, 48, 96 and 160 points, centred; and "H" at 96 points aligned top and bottom in a tall box.
import shutil
w = Wallpaper('text-vertical', 'text placement by point size', clear='0.1 0.12 0.18')
os.makedirs(os.path.join(HERE, 'text-vertical', 'fonts'), exist_ok=True)
shutil.copy('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', os.path.join(HERE, 'text-vertical', 'fonts', 'DejaVuSans.ttf'))
cells = [("H", 20, 150, 800), ("H", 48, 350, 800), ("H", 96, 620, 800), ("H", 160, 960, 800),
         ("Hg", 20, 150, 450), ("Hg", 48, 350, 450), ("Hg", 96, 620, 450), ("Hg", 160, 960, 450)]
for k, (text, pointsize, x, y) in enumerate(cells):
    w.add({"name": "cell %d" % k, "text": text, "font": "fonts/DejaVuSans.ttf", "pointsize": pointsize,
           "size": "400 300", "origin": "%d %d 0" % (x, y), "angles": "0 0 0", "scale": "1 1 1", "alpha": 1,
           "color": "1 0.85 0.3", "horizontalalign": "center", "verticalalign": "center", "padding": "0 0",
           "alignment": "center", "visible": True, "opaquebackground": False})
for k, (vertical, x) in enumerate((("top", 1350), ("bottom", 1700))):
    w.add({"name": "aligned %s" % vertical, "text": "H", "font": "fonts/DejaVuSans.ttf", "pointsize": 96,
           "size": "300 700", "origin": "%d 540 0" % x, "angles": "0 0 0", "scale": "1 1 1", "alpha": 1,
           "color": "0.5 0.9 1", "horizontalalign": "center", "verticalalign": vertical, "padding": "0 0",
           "alignment": "center", "visible": True, "opaquebackground": True, "backgroundcolor": "0.2 0.25 0.35"})
w.save()
with open(os.path.join(HERE, 'text-vertical', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'text-vertical'))

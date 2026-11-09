# text-outline-place: text-outline's 96-point row ("WgH 12", centred, outline 6) has its H 3 px left of Wallpaper Engine's
# while the W, g and 1 match, and its 48-point rows match. Is it the outline or the size? "WgH 12" centred at 96 points with
# no outline and with thickness 6 and 15, and at 48 points with thickness 6.
import shutil
w = Wallpaper('text-outline-place', 'where outlined letters sit', clear='0.1 0.12 0.18')
os.makedirs(os.path.join(HERE, w.name, 'fonts'), exist_ok=True)
shutil.copy('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', os.path.join(HERE, w.name, 'fonts', 'DejaVuSans.ttf'))
for i, (pointsize, thickness) in enumerate(((96, 0), (96, 6), (96, 15), (48, 6))):
    w.add({"name": '%d t%d' % (pointsize, thickness), "text": "WgH 12", "font": "fonts/DejaVuSans.ttf",
           "pointsize": pointsize, "size": "1400 240", "origin": "960 %d 0" % (930 - i * 250), "angles": "0 0 0",
           "scale": "1 1 1", "alpha": 1, "color": "1 0.85 0.3", "horizontalalign": "center", "verticalalign": "center",
           "padding": "0 0", "alignment": "center", "visible": True, "opaquebackground": False,
           "outline": thickness > 0, "outlinecolor": "0.6 0.05 0.05", "outlinethickness": thickness})
w.save()
with open(os.path.join(HERE, w.name, 'compare.json'), 'w') as f:
    json.dump({"probe": True, "fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, w.name))

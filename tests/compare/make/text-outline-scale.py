# text-outline-scale: Wallpaper Engine draws an outlined letter ~1.3 % larger than the same letter plain (text-outline-run:
# outlined H's stems 154 px apart where plain ones are 152, from the same pens). One big H at 200 points in DejaVu Sans,
# plain (left) and with a thickness 2 outline (right), left-aligned, so the white letter's edges give its scale and place.
import shutil
w = Wallpaper('text-outline-scale', 'size of an outlined letter', clear='0 0 0')
os.makedirs(os.path.join(HERE, w.name, 'fonts'), exist_ok=True)
shutil.copy('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', os.path.join(HERE, w.name, 'fonts', 'DejaVuSans.ttf'))
for i, thickness in enumerate((0, 2, 8)):
    w.add({"name": 'H t%d' % thickness, "text": "H", "font": "fonts/DejaVuSans.ttf", "pointsize": 200,
           "size": "600 700", "origin": "%d 540 0" % (330 + i * 620), "angles": "0 0 0", "scale": "1 1 1", "alpha": 1,
           "color": "1 1 1", "horizontalalign": "left", "verticalalign": "center", "padding": "0 0",
           "alignment": "center", "visible": True, "opaquebackground": False,
           "outline": thickness > 0, "outlinecolor": "1 0 0", "outlinethickness": thickness})
w.save()
with open(os.path.join(HERE, w.name, 'compare.json'), 'w') as f:
    json.dump({"probe": True, "fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, w.name))

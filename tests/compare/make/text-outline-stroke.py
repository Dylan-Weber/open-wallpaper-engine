# text-outline-stroke: the outline's shape at several thicknesses and point sizes. The letters have square outer corners
# (H, L), concave inner corners (H, L), sharp points (V, <, A) and a round letter (O). Left column "HLV<A" at 36
# points: thickness 1, 3, 8 and 20; right column "HLV O": thickness 4 at 16 points, 2.5 at 48, 4 and 12 at 60. DejaVu Sans is shipped with the
# wallpaper, so both engines draw the same glyphs.
import shutil
w = Wallpaper('text-outline-stroke', 'text outline shape', clear='0.1 0.12 0.18')
os.makedirs(os.path.join(HERE, 'text-outline-stroke', 'fonts'), exist_ok=True)
shutil.copy('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',
            os.path.join(HERE, 'text-outline-stroke', 'fonts', 'DejaVuSans.ttf'))
rows = (('t1', 520, 910, 1, 36), ('t3', 520, 740, 3, 36), ('t8', 520, 540, 8, 36), ('t20', 520, 330, 20, 36),
        ('p16', 1420, 990, 4, 16), ('p48', 1420, 830, 2.5, 48), ('p60', 1420, 600, 4, 60), ('p60t12', 1420, 330, 12, 60))
for name, x, y, thickness, pointsize in rows:
    w.add({"name": name, "text": "HLV<A" if x < 960 else "HLV O", "font": "fonts/DejaVuSans.ttf", "pointsize": pointsize,
           "size": "900 300", "origin": "%d %d 0" % (x, y), "angles": "0 0 0", "scale": "1 1 1", "alpha": 1,
           "color": "1 0.85 0.3", "horizontalalign": "center", "verticalalign": "center", "padding": "0 0",
           "alignment": "center", "visible": True, "opaquebackground": False,
           "outline": True, "outlinecolor": "0.6 0.05 0.05", "outlinethickness": thickness})
w.save()
with open(os.path.join(HERE, 'text-outline-stroke', 'compare.json'), 'w') as f:
    json.dump({"probe": True, "fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'text-outline-stroke'))

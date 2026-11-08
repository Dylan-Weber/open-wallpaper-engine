# text-outline: text with outlines (Ocarina of Time's clock has a 2-thick dark red outline that owe didn't draw). The font
# is shipped with the wallpaper (DejaVu Sans), so both engines draw the same glyphs. Left column at 48 points: no
# outline, outline thickness 2, 6 and 15; right column: thickness 6 at 20 and at 96 points, and a translucent layer
# (alpha 0.5) with a thickness 15 outline.
import shutil
w = Wallpaper('text-outline', 'text outlines', clear='0.1 0.12 0.18')
os.makedirs(os.path.join(HERE, 'text-outline', 'fonts'), exist_ok=True)
shutil.copy('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', os.path.join(HERE, 'text-outline', 'fonts', 'DejaVuSans.ttf'))
rows = (('none', 480, 900, False, 0, 48, 1), ('thin', 480, 700, True, 2, 48, 1), ('mid', 480, 500, True, 6, 48, 1),
        ('thick', 480, 300, True, 15, 48, 1), ('small', 1400, 900, True, 6, 20, 1), ('big', 1400, 650, True, 6, 96, 1),
        ('translucent', 1400, 350, True, 15, 48, 0.5))
for name, x, y, outline, thickness, pointsize, alpha in rows:
    w.add({"name": name, "text": "WgH 12", "font": "fonts/DejaVuSans.ttf", "pointsize": pointsize,
           "size": "800 220", "origin": "%d %d 0" % (x, y), "angles": "0 0 0", "scale": "1 1 1", "alpha": alpha,
           "color": "1 0.85 0.3", "horizontalalign": "center", "verticalalign": "center", "padding": "0 0",
           "alignment": "center", "visible": True, "opaquebackground": False,
           "outline": outline, "outlinecolor": "0.6 0.05 0.05", "outlinethickness": thickness})
w.save()
with open(os.path.join(HERE, 'text-outline', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'text-outline'))

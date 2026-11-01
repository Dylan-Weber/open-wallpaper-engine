# text-baseline: where letters land to the sub-pixel, by point size: an "H" at 12 to 40 points (steps of 2), each alone
# in its cell, with the font shipped (DejaVu Sans). In text-vertical owe's letters landed 1-2 px from Wallpaper Engine's,
# by an amount that changed with the size (whole-pixel rounding of the baseline?).
import shutil
w = Wallpaper('text-baseline', 'letter placement by point size', clear='0 0 0')
os.makedirs(os.path.join(HERE, 'text-baseline', 'fonts'), exist_ok=True)
shutil.copy('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', os.path.join(HERE, 'text-baseline', 'fonts', 'DejaVuSans.ttf'))
for k, pointsize in enumerate(range(12, 42, 2)):
    x, y = 260 + 340 * (k % 5), 860 - 300 * (k // 5)
    w.add({"name": "H %d" % pointsize, "text": "H", "font": "fonts/DejaVuSans.ttf", "pointsize": pointsize,
           "size": "240 240", "origin": "%d %d 0" % (x, y), "angles": "0 0 0", "scale": "1 1 1", "alpha": 1,
           "color": "1 1 1", "horizontalalign": "center", "verticalalign": "center", "padding": "0 0",
           "alignment": "center", "visible": True, "opaquebackground": False})
w.save()
with open(os.path.join(HERE, 'text-baseline', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'text-baseline'))

# text-padding-pair: text padding written as a pair ("x y", as Ocarina of Time's text layers have it) against a
# number: 20, "20 20", "40 0", "0 40" and "40 10", each around one word with a background box.
import shutil
w = Wallpaper('text-padding-pair', 'text padding as a pair', clear='0.1 0.12 0.18')
os.makedirs(os.path.join(HERE, 'text-padding-pair', 'fonts'), exist_ok=True)
shutil.copy('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',
            os.path.join(HERE, 'text-padding-pair', 'fonts', 'DejaVuSans.ttf'))
for k, padding in enumerate((20, "20 20", "40 0", "0 40", "40 10", "0 0")):
    w.add({"name": "padding %d" % k, "text": "Pad", "font": "fonts/DejaVuSans.ttf", "pointsize": 30,
           "size": "300 200", "origin": "%d %d 0" % (360 + 600 * (k % 3), 760 - 400 * (k // 3)), "angles": "0 0 0",
           "scale": "1 1 1", "alpha": 1, "color": "1 0.85 0.3", "horizontalalign": "center", "verticalalign": "top",
           "padding": padding, "alignment": "center", "visible": True, "opaquebackground": True,
           "backgroundcolor": "0.2 0.25 0.45"})
w.save()
with open(os.path.join(HERE, 'text-padding-pair', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'text-padding-pair'))

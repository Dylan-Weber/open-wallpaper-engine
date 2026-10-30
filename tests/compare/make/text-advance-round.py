# text-advance-round: how Wallpaper Engine rounds letter advances: seven M (DejaVu Sans, advance 0.8628 em) and ten i
# (0.2778 em), left-aligned at 31, 40, 47 and 53 points, so each row's letters show where their pens fall.
import shutil
w = Wallpaper('text-advance-round', 'letter advance rounding', clear='0.1 0.12 0.18')
os.makedirs(os.path.join(HERE, 'text-advance-round', 'fonts'), exist_ok=True)
shutil.copy('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',
            os.path.join(HERE, 'text-advance-round', 'fonts', 'DejaVuSans.ttf'))
for k, size in enumerate((31, 40, 47, 53)):
    for col, (text, x) in enumerate((("MMMMMMM", 60), ("iiiiiiiiii", 1500))):
        w.add({"name": "%s %d" % (text[0], size), "text": text, "font": "fonts/DejaVuSans.ttf", "pointsize": size,
               "size": "1400 200", "origin": "%d %d 0" % (x, 900 - 160 * k), "angles": "0 0 0", "scale": "1 1 1",
               "alpha": 1, "color": "1 0.85 0.3", "horizontalalign": "left", "verticalalign": "top", "padding": 0,
               "visible": True, "opaquebackground": False})
w.save()
with open(os.path.join(HERE, 'text-advance-round', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'text-advance-round'))

# text-align-vertical: where Wallpaper Engine puts a line of text in a tall box (400 high, no background), aligned top,
# centre and bottom, at 48 and 96 points (DejaVu Sans shipped with the wallpaper). Thin lines mark each box's top and
# bottom. owe's centred letters sat 0.04 x the point size higher than Wallpaper Engine's (text-baseline).
import shutil
w = Wallpaper('text-align-vertical', 'text aligned in a tall box', clear='0 0 0')
os.makedirs(os.path.join(HERE, 'text-align-vertical', 'fonts'), exist_ok=True)
shutil.copy('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',
            os.path.join(HERE, 'text-align-vertical', 'fonts', 'DejaVuSans.ttf'))
for k, (vertical, pointsize) in enumerate((('top', 48), ('center', 48), ('bottom', 48),
                                           ('top', 96), ('center', 96), ('bottom', 96))):
    x = 160 + 320 * k
    for edge in (340, 740):
        w.add({"name": "edge %d %d" % (k, edge), "image": "models/util/solidlayer.json", "size": "220 2",
               "color": "0.3 0.3 1", "alpha": 1, "origin": "%d %d 0" % (x, edge), "angles": "0 0 0",
               "scale": "1 1 1", "alignment": "center", "visible": True})
    w.add({"name": "%s %d" % (vertical, pointsize), "text": "H", "font": "fonts/DejaVuSans.ttf",
           "pointsize": pointsize, "size": "220 400", "origin": "%d 540 0" % x, "angles": "0 0 0",
           "scale": "1 1 1", "alpha": 1, "color": "1 1 1", "horizontalalign": "center", "verticalalign": vertical,
           "padding": "0 0", "alignment": "center", "visible": True, "opaquebackground": False})
w.save()
with open(os.path.join(HERE, 'text-align-vertical', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'text-align-vertical'))

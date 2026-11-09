# text-pair-advance: spaces at the ends of a line. Wallpaper Engine draws " H" (left-aligned) with the H at the box's left
# edge, owe one space advance further right. 48-point DejaVu Sans (shipped with the wallpaper): columns left-, centre- and
# right-aligned, rows "H", " H", "  H", "H ", "H  ", " H ", "H H" (a space inside stays), each over a dim box.
import shutil
w = Wallpaper('text-pair-advance', 'spaces at the ends of a line', clear='0 0 0')
os.makedirs(os.path.join(HERE, w.name, 'fonts'), exist_ok=True)
shutil.copy('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', os.path.join(HERE, w.name, 'fonts', 'DejaVuSans.ttf'))
TEXTS = ('H', ' H', '  H', 'H ', 'H  ', ' H ', 'H H')
for column, align in enumerate(('left', 'center', 'right')):
    for row, text in enumerate(TEXTS):
        x, y = 330 + column * 630, 960 - row * 140
        w.solid('box %s %d' % (align, row), x, y, 560, 110, color='0.15 0.15 0.3')
        w.add({"name": '%s %r' % (align, text), "text": text, "font": "fonts/DejaVuSans.ttf", "pointsize": 48,
               "size": "560 110", "origin": "%d %d 0" % (x, y), "angles": "0 0 0", "scale": "1 1 1", "alpha": 1,
               "color": "1 1 1", "horizontalalign": align, "verticalalign": "center", "padding": "0 0",
               "alignment": "center", "visible": True, "opaquebackground": False})
w.save()
with open(os.path.join(HERE, w.name, 'compare.json'), 'w') as f:
    json.dump({"probe": True, "fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, w.name))

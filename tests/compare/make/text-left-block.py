# text-left-block: OoT's clock (skinned-3737268876, "Clock on Screen": three left-aligned lines in a 268 x 105 box, outline
# 2) sits ~19 px right of Wallpaper Engine's in owe, every line alike. Three left-aligned lines in DejaVu Sans at 32 points
# in a box narrower than the text and in a wide one, each with and without an outline; a dim solid layer marks each box.
import shutil
w = Wallpaper('text-left-block', 'left-aligned text block in narrow and wide boxes', clear='0 0 0')
os.makedirs(os.path.join(HERE, w.name, 'fonts'), exist_ok=True)
shutil.copy('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', os.path.join(HERE, w.name, 'fonts', 'DejaVuSans.ttf'))
TEXT = "PM 12:00:00\nSep. 09, 2026\nFriday"
for i, (width, outline) in enumerate(((180, False), (180, True), (700, False), (700, True))):
    x, y = 480 + (i % 2) * 960, 780 - (i // 2) * 420
    w.solid('box %d' % i, x, y, width, 150, color='0.12 0.12 0.25')
    w.add({"name": 'text %d' % i, "text": TEXT, "font": "fonts/DejaVuSans.ttf", "pointsize": 32,
           "size": "%d 150" % width, "origin": "%d %d 0" % (x, y), "angles": "0 0 0", "scale": "1 1 1", "alpha": 1,
           "color": "1 0.2 0.2", "horizontalalign": "left", "verticalalign": "center", "padding": "0 0",
           "alignment": "center", "visible": True, "opaquebackground": False, "maxwidth": 500, "limitwidth": False,
           "outline": outline, "outlinecolor": "0.35 0 0", "outlinethickness": 2})
w.save()
with open(os.path.join(HERE, w.name, 'compare.json'), 'w') as f:
    json.dump({"probe": True, "fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, w.name))

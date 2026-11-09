# text-outline-run: Wallpaper Engine moves outlined letters up to ~3 px from its plain placement, only between letters of
# a line (single letters match: text-outline-letter). Runs of one letter, so every pen is a multiple of its advance:
# "HHHHHH", "IIIIIIIII" and "nnnnnnnn" left-aligned at 96 points in DejaVu Sans, each plain and with a thickness 6
# outline; the stems' places along the row say how the outlined pens move.
import shutil
w = Wallpaper('text-outline-run', 'runs of one outlined letter', clear='0 0 0')
os.makedirs(os.path.join(HERE, w.name, 'fonts'), exist_ok=True)
shutil.copy('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', os.path.join(HERE, w.name, 'fonts', 'DejaVuSans.ttf'))
ROWS = (('HHHHHH', 0), ('HHHHHH', 6), ('IIIIIIIII', 0), ('IIIIIIIII', 6), ('nnnnnnnn', 0), ('nnnnnnnn', 6))
for row, (text, thickness) in enumerate(ROWS):
    y = 990 - row * 170
    w.add({"name": '%s t%d' % (text[0], thickness), "text": text, "font": "fonts/DejaVuSans.ttf", "pointsize": 96,
           "size": "1880 160", "origin": "960 %d 0" % y, "angles": "0 0 0", "scale": "1 1 1", "alpha": 1,
           "color": "1 1 1", "horizontalalign": "left", "verticalalign": "center", "padding": "0 0",
           "alignment": "center", "visible": True, "opaquebackground": False,
           "outline": thickness > 0, "outlinecolor": "1 0 0", "outlinethickness": thickness})
w.save()
with open(os.path.join(HERE, w.name, 'compare.json'), 'w') as f:
    json.dump({"probe": True, "fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, w.name))

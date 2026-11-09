# text-outline-letter: Wallpaper Engine moves outlined letters by up to ~3 px from where its plain text puts them (text-outline-place,
# the same at thickness 6 and 15). Single letters, each in a left-aligned layer of its own (so the pen starts at 0), at 96 points in
# DejaVu Sans: the top row plain, the bottom row with a thickness 6 outline. text_letter_read.py measures each letter's ink.
import shutil
w = Wallpaper('text-outline-letter', 'outlined letters one by one', clear='0 0 0')
os.makedirs(os.path.join(HERE, w.name, 'fonts'), exist_ok=True)
shutil.copy('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', os.path.join(HERE, w.name, 'fonts', 'DejaVuSans.ttf'))
LETTERS = 'WgH12IO'
for row, thickness in enumerate((0, 6)):
    for i, letter in enumerate(LETTERS):
        x, y = 140 + i * 250, 760 - row * 420
        w.add({"name": '%s t%d' % (letter, thickness), "text": letter, "font": "fonts/DejaVuSans.ttf", "pointsize": 96,
               "size": "240 300", "origin": "%d %d 0" % (x + 120, y), "angles": "0 0 0", "scale": "1 1 1", "alpha": 1,
               "color": "1 1 1", "horizontalalign": "left", "verticalalign": "center", "padding": "0 0",
               "alignment": "center", "visible": True, "opaquebackground": False,
               "outline": thickness > 0, "outlinecolor": "1 0 0", "outlinethickness": thickness})
w.save()
with open(os.path.join(HERE, w.name, 'compare.json'), 'w') as f:
    json.dump({"probe": True, "fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, w.name))

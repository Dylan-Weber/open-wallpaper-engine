# text-outline-sweep: how Wallpaper Engine sizes and places an outlined letter's image (text-outline-scale: at 200 points the
# outlined H is 2.2 % wider at thickness 2 and 0.7 % narrower and 7 px right at 8, letter and outline moving together). One
# wallpaper a point size (25, 50, 100, 200), each a row of H's at outline thickness 0, 1, 2, 4, 8 and 16, white letters with
# red outlines, each in a left-aligned layer of its own whose origin is 300 units right of the last: the white letter's
# stems give its box (text_outline_sweep_read.py).
import shutil
SIZES = (25, 50, 100, 200)
THICKNESSES = (0, 1, 2, 4, 8, 16)
for size in SIZES:
    name = 'text-outline-sweep-%d' % size
    w = Wallpaper(name, 'outlined letters at %d points' % size, clear='0 0 0')
    os.makedirs(os.path.join(HERE, name, 'fonts'), exist_ok=True)
    shutil.copy('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', os.path.join(HERE, name, 'fonts', 'DejaVuSans.ttf'))
    for col, thickness in enumerate(THICKNESSES):
        w.add({"name": 'H t%d' % thickness, "text": "H", "font": "fonts/DejaVuSans.ttf", "pointsize": size,
               "size": "280 400", "origin": "%d 540 0" % (100 + col * 300), "angles": "0 0 0", "scale": "1 1 1",
               "alpha": 1, "color": "1 1 1", "horizontalalign": "left", "verticalalign": "center", "padding": "0 0",
               "alignment": "center", "visible": True, "opaquebackground": False, "outline": thickness > 0,
               "outlinecolor": "1 0 0", "outlinethickness": thickness})
    w.save()
    with open(os.path.join(HERE, name, 'compare.json'), 'w') as f:
        json.dump({"probe": True, "fps": 30}, f)
    clock_probe.stamp(os.path.join(HERE, name))

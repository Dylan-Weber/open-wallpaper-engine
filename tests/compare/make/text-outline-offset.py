# text-outline-offset: Wallpaper Engine draws an outlined letter at an offset from its plain place that grows with size and
# doesn't depend on the thickness (text-outline-sweep: +0.4 px at 25 points, +1.6 at 50). One wallpaper a case, so big
# letters have room: a plain letter (layer origin x 200) and the same letter with a thickness 4 outline (origin x 1100), both
# left-aligned and centred at y 540. H at 25, 50, 100, 200 and 400 points; O and I at 100 (does it depend on the glyph?).
import shutil
CASES = (('H', 25), ('H', 50), ('H', 100), ('H', 200), ('H', 400), ('O', 100), ('I', 100))
for letter, size in CASES:
    name = 'text-outline-offset-%s%d' % (letter.lower(), size)
    w = Wallpaper(name, 'outlined %s at %d points' % (letter, size), clear='0 0 0')
    os.makedirs(os.path.join(HERE, name, 'fonts'), exist_ok=True)
    shutil.copy('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', os.path.join(HERE, name, 'fonts', 'DejaVuSans.ttf'))
    for x, thickness in ((200, 0), (1100, 4)):
        w.add({"name": '%s t%d' % (letter, thickness), "text": letter, "font": "fonts/DejaVuSans.ttf", "pointsize": size,
               "size": "800 1000", "origin": "%d 540 0" % x, "angles": "0 0 0", "scale": "1 1 1", "alpha": 1,
               "color": "1 1 1", "horizontalalign": "left", "verticalalign": "center", "padding": "0 0",
               "alignment": "center", "visible": True, "opaquebackground": False, "outline": thickness > 0,
               "outlinecolor": "1 0 0", "outlinethickness": thickness})
    w.save()
    with open(os.path.join(HERE, name, 'compare.json'), 'w') as f:
        json.dump({"probe": True, "fps": 30}, f)
    clock_probe.stamp(os.path.join(HERE, name))

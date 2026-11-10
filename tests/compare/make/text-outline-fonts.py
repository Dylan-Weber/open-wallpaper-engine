# text-outline-fonts: does Wallpaper Engine's outlined-glyph cell care which font a glyph is from? #197 found WE makes one
# cell per glyph at the first size it draws it; its cache may be keyed by glyph index alone. DejaVu Sans and Serif number
# their ASCII glyphs alike (H is the same index) but draw them differently (serifs), so a Serif H drawn from a Sans cell
# is easy to tell. Three wallpapers, "HLV O" with a thickness 4 outline: -a Serif at 60 points alone; -b Sans at 16 points,
# then Serif at 60 (the 60-point row drawn after the 16-point one, as in text-outline-first-size); -c Serif at 16, then Sans at 60.
import shutil
FONTS = {'sans': 'DejaVuSans.ttf', 'serif': 'DejaVuSerif.ttf'}
CASES = (('a', (('serif', 60),)),
         ('b', (('sans', 16), ('serif', 60))),
         ('c', (('serif', 16), ('sans', 60))))
for suffix, rows in CASES:
    name = 'text-outline-fonts-' + suffix
    w = Wallpaper(name, 'outlined text in two fonts', clear='0.1 0.12 0.18')
    os.makedirs(os.path.join(HERE, name, 'fonts'), exist_ok=True)
    for font in {font for font, _ in rows}:
        shutil.copy('/usr/share/fonts/truetype/dejavu/' + FONTS[font], os.path.join(HERE, name, 'fonts', FONTS[font]))
    for font, size in rows:
        w.add({"name": '%s %d' % (font, size), "text": "HLV O", "font": "fonts/" + FONTS[font], "pointsize": size,
               "size": "900 300", "origin": "1420 %d 0" % (990 if size == 16 else 600), "angles": "0 0 0",
               "scale": "1 1 1", "alpha": 1, "color": "1 0.85 0.3", "horizontalalign": "center",
               "verticalalign": "center", "padding": "0 0", "alignment": "center", "visible": True,
               "opaquebackground": False, "outline": True, "outlinecolor": "0.6 0.05 0.05", "outlinethickness": 4})
    w.save()
    with open(os.path.join(HERE, name, 'compare.json'), 'w') as f:
        json.dump({"probe": True, "fps": 30}, f)
    clock_probe.stamp(os.path.join(HERE, name))

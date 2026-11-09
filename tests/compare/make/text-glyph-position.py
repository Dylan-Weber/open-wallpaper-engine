# text-glyph-position: where a glyph's ink lands, size by size (text-outline's letters sit ~0.5 px right in owe at 48-96 pt,
# with or without an outline). Left-aligned "IlH" in DejaVu Sans (shipped with the wallpaper) at 16 to 96 points, each in a
# box whose left edge is at x = 200; text_glyph_read.py measures each row's first stem (the I) to a hundredth of a pixel.
import shutil
w = Wallpaper('text-glyph-position', 'where glyph ink lands by size', clear='0 0 0')
os.makedirs(os.path.join(HERE, w.name, 'fonts'), exist_ok=True)
shutil.copy('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', os.path.join(HERE, w.name, 'fonts', 'DejaVuSans.ttf'))
SIZES = (16, 24, 36, 48, 60, 72, 96)
y = 980
for size in SIZES:
    height = size * 2
    y -= height // 2 + 10
    w.add({"name": "I %d" % size, "text": "IlH", "font": "fonts/DejaVuSans.ttf", "pointsize": size,
           "size": "%d %d" % (1400, height), "origin": "%d %d 0" % (200 + 700, y), "angles": "0 0 0", "scale": "1 1 1",
           "alpha": 1, "color": "1 1 1", "horizontalalign": "left", "verticalalign": "center", "padding": "0 0",
           "alignment": "center", "visible": True, "opaquebackground": False})
    y -= height // 2
w.save()
with open(os.path.join(HERE, w.name, 'compare.json'), 'w') as f:
    json.dump({"probe": True, "fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, w.name))

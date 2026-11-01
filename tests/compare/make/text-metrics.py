# text-metrics: where text and its background box sit for fonts whose vertical metrics differ one table at a time.
# Each font's "H" is a square 100..600 x 0..700 units in a 1000-unit em; the fonts set (hhea ascent/descent/gap,
# OS/2 typo ascent/descent/gap, OS/2 win ascent/descent): each column one font, centred above, top-aligned below.
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen
FONTS = {  # name: (hhea, typo, win)
    'base': ((800, -200, 0), (800, -200, 0), (800, 200)),
    'tall': ((1000, 0, 0), (1000, 0, 0), (1000, 0)),
    'deep': ((500, -500, 0), (500, -500, 0), (500, 500)),
    'win': ((800, -200, 0), (800, -200, 0), (1100, 400)),
    'hhea': ((1100, -400, 0), (800, -200, 0), (800, 200)),
    'typo': ((800, -200, 0), (1100, -400, 0), (800, 200)),
    'gap': ((800, -200, 300), (800, -200, 300), (800, 200)),
}
w = Wallpaper('text-metrics', 'text placement by font metrics', clear='0.1 0.12 0.18')
os.makedirs(os.path.join(HERE, 'text-metrics', 'fonts'), exist_ok=True)


def square(box):
    pen = TTGlyphPen(None)
    if box:
        x0, y0, x1, y1 = box
        pen.moveTo((x0, y0)); pen.lineTo((x0, y1)); pen.lineTo((x1, y1)); pen.lineTo((x1, y0)); pen.closePath()
    return pen.glyph()


for name, (hhea, typo, win) in FONTS.items():
    b = FontBuilder(1000, isTTF=True)
    b.setupGlyphOrder(['.notdef', 'space', 'H'])
    b.setupCharacterMap({0x20: 'space', ord('H'): 'H'})
    b.setupGlyf({'.notdef': square((100, 0, 600, 700)), 'space': square(None), 'H': square((100, 0, 600, 700))})
    b.setupHorizontalMetrics({'.notdef': (700, 100), 'space': (300, 0), 'H': (700, 100)})
    b.setupHorizontalHeader(ascent=hhea[0], descent=hhea[1], lineGap=hhea[2])
    b.setupNameTable({'familyName': 'Metrics %s' % name, 'styleName': 'Regular'})
    b.setupOS2(sTypoAscender=typo[0], sTypoDescender=typo[1], sTypoLineGap=typo[2], usWinAscent=win[0],
               usWinDescent=win[1])
    b.setupPost()
    b.save(os.path.join(HERE, 'text-metrics', 'fonts', '%s.ttf' % name))
for k, name in enumerate(FONTS):
    x = 150 + 270 * k
    for row, (vertical, y) in enumerate((("center", 780), ("top", 420))):
        w.add({"name": "%s %s" % (name, vertical), "text": "H", "font": "fonts/%s.ttf" % name, "pointsize": 30,
               "size": "200 250", "origin": "%d %d 0" % (x, y), "angles": "0 0 0", "scale": "1 1 1", "alpha": 1,
               "color": "1 0.85 0.3", "horizontalalign": "center", "verticalalign": vertical, "padding": "0 0",
               "alignment": "center", "visible": True, "opaquebackground": True,
               "backgroundcolor": "0.2 0.25 0.45"})
w.save()
with open(os.path.join(HERE, 'text-metrics', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'text-metrics'))

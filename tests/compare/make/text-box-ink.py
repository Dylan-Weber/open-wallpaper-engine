# text-box-ink: a background box around a letter that rises above its font's ascent (an H 0.7 em tall in a font of
# ascent and descent 0.5 em), beside one that doesn't, aligned top and bottom, without and with padding.
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen
w = Wallpaper('text-box-ink', 'a background box around ink above the line', clear='0.1 0.12 0.18')
os.makedirs(os.path.join(HERE, 'text-box-ink', 'fonts'), exist_ok=True)


def square(box):
    pen = TTGlyphPen(None)
    if box:
        x0, y0, x1, y1 = box
        pen.moveTo((x0, y0)); pen.lineTo((x0, y1)); pen.lineTo((x1, y1)); pen.lineTo((x1, y0)); pen.closePath()
    return pen.glyph()


for name, (ascent, descent) in (('deep', (500, 500)), ('base', (800, 200))):
    b = FontBuilder(1000, isTTF=True)
    b.setupGlyphOrder(['.notdef', 'space', 'H'])
    b.setupCharacterMap({0x20: 'space', ord('H'): 'H'})
    b.setupGlyf({'.notdef': square((100, 0, 600, 700)), 'space': square(None), 'H': square((100, 0, 600, 700))})
    b.setupHorizontalMetrics({'.notdef': (700, 100), 'space': (300, 0), 'H': (700, 100)})
    b.setupHorizontalHeader(ascent=ascent, descent=-descent, lineGap=0)
    b.setupNameTable({'familyName': 'Ink %s' % name, 'styleName': 'Regular'})
    b.setupOS2(sTypoAscender=ascent, sTypoDescender=-descent, sTypoLineGap=0, usWinAscent=ascent,
               usWinDescent=descent)
    b.setupPost()
    b.save(os.path.join(HERE, 'text-box-ink', 'fonts', '%s.ttf' % name))
cells = [(font, vertical, padding) for font in ('deep', 'base') for vertical in ('top', 'bottom') for padding in (0, 15)]
for k, (font, vertical, padding) in enumerate(cells):
    w.add({"name": "%s %s %d" % (font, vertical, padding), "text": "H", "font": "fonts/%s.ttf" % font,
           "pointsize": 40, "size": "300 300", "origin": "%d %d 0" % (150 + 230 * k, 640 if vertical == 'top' else 440),
           "angles": "0 0 0", "scale": "1 1 1", "alpha": 1, "color": "1 0.85 0.3", "horizontalalign": "center",
           "verticalalign": vertical, "padding": padding, "alignment": "center", "visible": True,
           "opaquebackground": True, "backgroundcolor": "0.2 0.25 0.45"})
w.save()
with open(os.path.join(HERE, 'text-box-ink', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'text-box-ink'))

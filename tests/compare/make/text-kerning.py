# text-kerning: kerned pairs (AV, To, Te, Wa, LT) in DejaVu Sans with only its legacy 'kern' table, only its GPOS
# kerning, and neither, so the rows show which kerning Wallpaper Engine applies.
from fontTools.ttLib import TTFont
w = Wallpaper('text-kerning', 'kerning from the kern table and from GPOS', clear='0.1 0.12 0.18')
os.makedirs(os.path.join(HERE, 'text-kerning', 'fonts'), exist_ok=True)
for name, drop in (('kern', ['GPOS']), ('gpos', ['kern']), ('none', ['kern', 'GPOS'])):
    font = TTFont('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf')
    for table in drop:
        del font[table]
    font['name'].setName('DejaVu Sans %s' % name, 1, 3, 1, 0x409)
    font.save(os.path.join(HERE, 'text-kerning', 'fonts', '%s.ttf' % name))
for k, name in enumerate(('kern', 'gpos', 'none')):
    w.add({"name": name, "text": "AVATAR To Te Wa LT", "font": "fonts/%s.ttf" % name, "pointsize": 36,
           "size": "1700 200", "origin": "%d %d 0" % (120, 860 - 300 * k), "angles": "0 0 0", "scale": "1 1 1",
           "alpha": 1, "color": "1 0.85 0.3", "horizontalalign": "left", "verticalalign": "top", "padding": 0,
           "visible": True, "opaquebackground": False})
w.save()
with open(os.path.join(HERE, 'text-kerning', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'text-kerning'))

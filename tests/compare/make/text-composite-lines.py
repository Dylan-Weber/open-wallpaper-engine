# text-composite-lines: where Wallpaper Engine puts multi-line text inside a sampled text layer's composite (#163: OoT's
# three-line clock sits 2-3 px lower in owe than in WE, while one line matches). Hidden left-aligned text layers (DejaVu Sans,
# 20 points, 268 x 105 boxes) of 1, 2, 3 and 5 lines ("H1", "H2", ...), aligned top, centre and bottom, each shown by its own
# fullscreen layer's blend effect (TRANSFORMUV) at a known offset: the composite's top left is at the offset.
import shutil
w = Wallpaper('text-composite-lines', 'multi-line text inside a sampled text layer\'s composite', clear='0 0 0')
os.makedirs(os.path.join(HERE, w.name, 'fonts'), exist_ok=True)
shutil.copy('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', os.path.join(HERE, w.name, 'fonts', 'DejaVuSans.ttf'))
LINES = (1, 2, 3, 5)
ALIGNS = ('top', 'center', 'bottom')
cases = []
for a, align in enumerate(ALIGNS):
    for l, lines in enumerate(LINES):
        cases.append(('\n'.join('H%d' % (i + 1) for i in range(lines)), align, (40 + l * 380, 20 + a * 280)))
text_ids = []
for i, (text, align, _) in enumerate(cases):
    text_ids.append(w.add({"name": "text %d" % i, "text": text, "font": "fonts/DejaVuSans.ttf", "pointsize": 20,
                           "size": "268 105", "origin": "960 540 0", "angles": "0 0 0", "scale": "1 1 1", "alpha": 1,
                           "color": "1 1 1", "horizontalalign": "left", "verticalalign": align, "padding": "0 0",
                           "visible": False, "opaquebackground": False}))
for i, (_, _, offset) in enumerate(cases):
    effect = shipped('blend', {"blendangle": 0.0, "blendoffset": "%d %d" % offset, "blendscale": 1, "multiply": 1.0},
                     {"BLENDMODE": 0, "TRANSFORMUV": 1}, id=100 + i)
    effect['passes'][0]['textures'] = [None, "_rt_imageLayerComposite_%d_a" % text_ids[i]]
    w.add({"name": "hud %d" % i, "image": "models/util/fullscreenlayer.json", "dependencies": [text_ids[i]],
           "parallaxDepth": "0 0", "origin": "960 540 0", "angles": "0 0 0", "scale": "1 1 1", "visible": True,
           "effects": [effect]})
w.save()
copy_shipped_effects(w, ['blend'])
with open(os.path.join(HERE, w.name, 'compare.json'), 'w') as f:
    json.dump({"probe": True, "fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, w.name))

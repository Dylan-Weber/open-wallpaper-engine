# text-composite-blend: OoT's on-screen clock (skinned-3737268876) is a hidden text layer shown by a fullscreen "HUD" layer
# whose blend effect (TRANSFORMUV, blendoffset 16 16, scale 1) samples the text layer's _rt_imageLayerComposite; owe draws
# "FRIDAY" ~19 px right of Wallpaper Engine. The blend effect places the composite by both textures' resolutions, so where
# the text lands says how big the text layer's composite is. Three hidden left-aligned text layers (DejaVu Sans), each shown
# by its own fullscreen blend layer: a 268 x 105 box with OoT's three lines, the same box with one short line, and a
# 600 x 105 box with the three lines.
import shutil
w = Wallpaper('text-composite-blend', 'a text layer shown through another layer\'s blend effect', clear='0 0 0')
os.makedirs(os.path.join(HERE, w.name, 'fonts'), exist_ok=True)
shutil.copy('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', os.path.join(HERE, w.name, 'fonts', 'DejaVuSans.ttf'))
CASES = (("PM 12:00:00\nOct. 09, 2026\nFriday", 268, (16, 16)), ("Fri", 268, (16, 300)), ("PM 12:00:00\nOct. 09, 2026\nFriday", 600, (16, 600)))
text_ids = []
for i, (text, width, _) in enumerate(CASES):
    text_ids.append(w.add({"name": "text %d" % i, "text": text, "font": "fonts/DejaVuSans.ttf", "pointsize": 20,
                           "size": "%d 105" % width, "origin": "960 540 0", "angles": "0 0 0", "scale": "1 1 1",
                           "alpha": 1, "color": "1 0.2 0.2", "horizontalalign": "left", "verticalalign": "center",
                           "padding": "0 0", "visible": False, "opaquebackground": False,
                           "outline": True, "outlinecolor": "0.35 0 0", "outlinethickness": 2}))
for i, (_, _, offset) in enumerate(CASES):
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

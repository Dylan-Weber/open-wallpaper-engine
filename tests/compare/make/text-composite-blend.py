# text-composite-blend: OoT's on-screen clock (skinned-3737268876) is a hidden left-aligned text layer shown by a fullscreen
# "HUD" layer whose blend effect (TRANSFORMUV, blendoffset 16 16, scale 1) samples the text layer's _rt_imageLayerComposite;
# owe drew "FRIDAY" ~19 px right of Wallpaper Engine. The blend effect puts the composite's top left at its offset, so where
# the text lands says where it sits in its composite. Hidden one-line text layers (DejaVu Sans, 20 points) in 268 x 105
# boxes: "Friday" left-, centre- and right-aligned and "Fri" left-aligned, each shown by its own fullscreen blend layer.
# (Lines wider or taller than the box: Wallpaper Engine doesn't cut them; not covered here.)
import shutil
w = Wallpaper('text-composite-blend', 'a text layer shown through another layer\'s blend effect', clear='0 0 0')
os.makedirs(os.path.join(HERE, w.name, 'fonts'), exist_ok=True)
shutil.copy('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', os.path.join(HERE, w.name, 'fonts', 'DejaVuSans.ttf'))
CASES = (("Friday", "left", (16, 16)), ("Friday", "center", (500, 16)), ("Friday", "right", (16, 300)),
         ("Fri", "left", (500, 300)))
text_ids = []
for i, (text, align, _) in enumerate(CASES):
    text_ids.append(w.add({"name": "text %d" % i, "text": text, "font": "fonts/DejaVuSans.ttf", "pointsize": 20,
                           "size": "268 105", "origin": "960 540 0", "angles": "0 0 0", "scale": "1 1 1",
                           "alpha": 1, "color": "1 0.2 0.2", "horizontalalign": align, "verticalalign": "center",
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

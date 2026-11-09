# text-3d-align: OoT's clock text ("Clock on Screen": a 3D scene's text layer at scale 0.001 under a turned group, left-
# aligned in a 268 x 105 box) sits ~19 px right of Wallpaper Engine's in owe, while left-aligned text in 2D scenes matches.
# A perspective scene with three text layers at scale 0.01 (boxes 268 x 105, so 2.68 units wide), left, centre and right
# aligned, each with a small cube at its origin; and a left-aligned one turned 30 degrees about y like OoT's clock group.
import shutil
w = Scene3D('text-3d-align', 'text alignment in a 3D scene', eye='0 0 12')
os.makedirs(os.path.join(HERE, w.name, 'fonts'), exist_ok=True)
shutil.copy('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', os.path.join(HERE, w.name, 'fonts', 'DejaVuSans.ttf'))
marker = w.material('marker', color='0.2 0.6 1')
for i, (align, y, yaw) in enumerate((('left', 2.5, 0), ('center', 0.5, 0), ('right', -1.5, 0), ('left', -3.5, 30))):
    w.cube('origin %d' % i, marker, 0, y, 0, scale='0.08 0.08 0.08')
    w.add({"name": '%s %d' % (align, i), "text": "PM 12:00:00\nFriday", "font": "fonts/DejaVuSans.ttf",
           "pointsize": 32, "size": "268 105", "origin": vec(0, y, 0), "angles": vec(0, math.radians(yaw), 0),
           "scale": "0.01 0.01 0.01", "alpha": 1, "color": "1 0.3 0.3", "horizontalalign": align,
           "verticalalign": "center", "padding": "0 0", "visible": True, "opaquebackground": True,
           "backgroundcolor": "0.1 0.1 0.3"})
w.save()
with open(os.path.join(HERE, w.name, 'compare.json'), 'w') as f:
    json.dump({"probe": True, "fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, w.name))

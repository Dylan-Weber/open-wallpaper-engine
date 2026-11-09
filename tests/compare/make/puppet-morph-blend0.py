# puppet-morph-blend0: strips whose blend shape (every vertex but the last column 40 up, alpha flag 0x1000) is driven by
# an animation layer that should add nothing or half of it: none (control), additive at blend 0, additive at 0.5, a
# normal layer at blend 0, and additive at blend 0 with a shape that has no 0x1000 flag. Wallpaper Engine marks a blend
# shape whose curve a visible layer samples as active whatever the layer's blend (FUN_1401fdf90, +0x398); Gun Girl
# (psim) draws a pixel or two differently in Wallpaper Engine when her additive Blink and Mood layers are there at blend 0.
w = Wallpaper('puppet-morph-blend0', 'blend shapes driven by layers at blend 0', clear='0 0 0')
columns = 9
vertices, indices = strip_mesh(columns=columns)
index = [0 if number % columns == columns - 1 else 1 + (number // columns) * (columns - 1) + number % columns
         for number in range(len(vertices))]
count = max(index)
still = [((0, 0, 0), (0, 0, 0), (1, 1, 1))] * 4
bones = [('root', None, (0, 0, 0)), ('end', 0, (300, 0, 0))]
up = {"offsets": [(0, 40, 0)] * count, "alpha": [1.0] * count, "normals": [(0, 0, 0)] * count}
cells = [('no layer', 0x1000, None), ('additive, blend 0', 0x1000, (True, 0.0)), ('additive, blend 0.5', 0x1000, (True, 0.5)),
         ('normal, blend 0', 0x1000, (False, 0.0)), ('additive, blend 0, no alpha flag', 0x0, (True, 0.0))]
for i, (title, flags, layer) in enumerate(cells):
    morph = {"index": index, "scale": 100.0, "flags": flags, "shapes": [up]}
    write_binary('puppet-morph-blend0/models/strip%d_puppet.mdl' % i, puppet_mdl(
        'materials/strip.json', vertices, indices, bones, [('shapes', 30.0, [still, still], [[1.0] * 4])],
        morph=morph))
    w.files['models/strip%d.json' % i] = {"material": "materials/strip.json", "puppet": "models/strip%d_puppet.mdl" % i}
    x, y = (560, 1360)[i % 2], 940 - 230 * (i // 2)
    w.add({"name": "strip %d: %s" % (i, title), "image": 'models/strip%d.json' % i, "origin": "%d %d 0" % (x, y),
           "size": "600 100", "color": "1 1 1", "alpha": 1, "angles": "0 0 0", "scale": "1 1 1", "alignment": "center",
           "animationlayers": [] if layer is None else [
               {"animation": 1, "additive": layer[0], "blend": layer[1], "blendin": False, "blendout": False,
                "blendtime": 0.5, "rate": 1.0, "visible": True, "id": 100, "name": "shapes"}]})
w.files['materials/strip.json'] = {"passes": [{"shader": "genericimage4", "blending": "translucent", "cullmode": "nocull",
                                              "depthtest": "disabled", "depthwrite": "disabled",
                                              "textures": ["gradient/gradient_rainbow"]}]}
w.save()
with open(os.path.join(HERE, 'puppet-morph-blend0', 'compare.json'), 'w') as f:
    json.dump({"probe": True, "fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'puppet-morph-blend0'))

# puppet-morph-alpha: white strips whose blend shapes carry a per-vertex alpha (mesh morph flag 0x1000) on black, so the
# brightness is the alpha. Wallpaper Engine's genericimage4 multiplies each morphed vertex's alpha by
# alpha * weight + (1 - weight) for every shape. The strips: an alpha ramp at weight 1 and at 0.5 (the last column is not
# morphed and stays opaque); two shapes, a flat half alpha and the ramp with an upward offset, multiplied; a shape with an
# offset but no 0x1000 flag (Wallpaper Engine leaves its morph texture empty: the strip doesn't move and its morphed
# vertices fade to alpha 0); and two strips with LIGHTING, one whose shape turns its normals (flag 0x400) and one
# without: an image layer's shader never reads the normal offsets, so both look alike (no point light: Wallpaper Engine
# didn't light these genericimage4 puppets with one, owe did; not a blend-shape matter).
w = Wallpaper('puppet-morph-alpha', "blend shapes' per-vertex alpha and normal offsets", clear='0 0 0')
columns = 9
vertices, indices = strip_mesh(columns=columns)
# the shapes move every vertex but the last column's two
index = [0 if number % columns == columns - 1 else 1 + (number // columns) * (columns - 1) + number % columns
         for number in range(len(vertices))]
count = max(index)
ramp = [(entry % (columns - 1)) / (columns - 1) for entry in range(count)]  # 0 .. 0.875 along the strip
still = [((0, 0, 0), (0, 0, 0), (1, 1, 1))] * 4
bones = [('root', None, (0, 0, 0)), ('end', 0, (300, 0, 0))]
tilt = (0.9 / 3.465, 0, -0.6 / 3.465)  # normal offset (the shader adds 3.465 times it): towards +x

def shape(alpha=None, up=0.0, normals=None):
    return {"offsets": [(0, up, 0)] * count, "alpha": alpha if alpha is not None else [1.0] * count,
            "normals": normals or [(0, 0, 0)] * count}

def weights(*values):
    return [[value] * 4 for value in values]

cells = [
    ('alpha ramp, weight 1', 0x1000, [shape(ramp)], weights(1.0), False),
    ('alpha ramp, weight 0.5', 0x1000, [shape(ramp)], weights(0.5), False),
    ('half alpha times the ramp, moved up', 0x1000, [shape([0.5] * count), shape(ramp, up=30)], weights(1.0, 1.0), False),
    ('offset without the alpha flag', 0x0, [shape(up=40)], weights(1.0), False),
    ('lit, normals turned', 0x1400, [shape(normals=[tilt] * count)], weights(1.0), True),
    ('lit, normals kept', 0x1000, [shape()], weights(1.0), True),
]
for i, (title, flags, shapes, curves, lit) in enumerate(cells):
    morph = {"index": index, "scale": 100.0, "flags": flags, "shapes": shapes}
    write_binary('puppet-morph-alpha/models/strip%d_puppet.mdl' % i, puppet_mdl(
        'materials/strip%d.json' % i, vertices, indices, bones, [('shapes', 30.0, [still, still], curves)],
        shading=lit, morph=morph))
    w.files['models/strip%d.json' % i] = {"material": "materials/strip%d.json" % i,
                                          "puppet": "models/strip%d_puppet.mdl" % i}
    material = {"shader": "genericimage4", "blending": "translucent", "cullmode": "nocull", "depthtest": "disabled",
                "depthwrite": "disabled", "textures": ["util/white"]}
    if lit:
        material["combos"] = {"LIGHTING": 1}
        material["constantshadervalues"] = {"roughness": 1.0, "metallic": 0.0}
    w.files['materials/strip%d.json' % i] = {"passes": [material]}
    x, y = (560, 1360)[i % 2], 960 - 200 * (i // 2)
    w.add({"name": "strip %d: %s" % (i, title), "image": 'models/strip%d.json' % i, "origin": "%d %d 0" % (x, y),
           "size": "600 100", "color": "1 1 1", "alpha": 1, "angles": "0 0 0", "scale": "1 1 1",
           "alignment": "center",
           "animationlayers": [{"animation": 1, "additive": False, "blend": 1.0, "blendin": False, "blendout": False,
                                "blendtime": 0.5, "rate": 1.0, "visible": True, "id": 100, "name": "shapes"}]})
w.save()
with open(os.path.join(HERE, 'puppet-morph-alpha', 'compare.json'), 'w') as f:
    json.dump({"probe": True, "fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'puppet-morph-alpha'))

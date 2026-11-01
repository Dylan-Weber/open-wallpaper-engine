# model-alphatocoverage-alpha: cubes whose material blends "alphatocoverage" with a material alpha of 0, 0.3 and 1,
# and (control) a translucent one with alpha 0, each in front of a blue cube. More Physics' arena walls are
# alphatocoverage with alpha 0: Wallpaper Engine draws nothing of them, owe drew them as dark boxes hiding the sky.
w = Scene3D('model-alphatocoverage-alpha', 'alphatocoverage materials with an alpha', ambient='1 1 1')
blue = w.material('blue', '0 0 1')
cells = (('a2c 0', 'alphatocoverage', 0.0), ('a2c 0.3', 'alphatocoverage', 0.3), ('a2c 1', 'alphatocoverage', 1.0),
         ('translucent 0', 'translucent', 0.0))
for i, (label, blending, alpha) in enumerate(cells):
    x = -6 + i * 4
    mat = w.material('red%d' % i, '1 0 0', blending=blending, alpha=alpha)
    w.cube('behind %d' % i, blue, x + 0.4, 0.3, -3, (0, 0.5, 0), '1.5 1.5 1.5')
    w.cube(label, mat, x, 0, 0, (0.3, 0.5, 0), '1.5 1.5 1.5')
# the first material also carries a capital-A "Alpha" of 1 beside its alpha of 0, as More Physics' wall material does
w.files['materials/red0.json']['passes'][0]['constantshadervalues']['Alpha'] = 1
w.save()
with open(os.path.join(HERE, 'model-alphatocoverage-alpha', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'model-alphatocoverage-alpha'))

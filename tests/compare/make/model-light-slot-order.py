# model-light-slot-order: four point lights by the cubes and then a sun, in that order, with a light config of four
# points and one directional: Wallpaper Engine's Lighting V1 (generic4) lights all five, not the first four (Ocarina of
# Time's courtyard lists its torches before its sun)
w = Scene3D('model-light-slot-order', 'four point lights listed before a sun', ambient='0.05 0.05 0.05')
lit = w.material('lit', '1 1 1', lighting=True)
for i, x in enumerate((-4.5, -1.5, 1.5, 4.5)):
    w.cube('cube %d' % i, lit, x, 0, 0, (0.4, 0.6, 0), '0.9 0.9 0.9')
    w.light('point %d' % i, 'lpoint', x, 0, 2.2, '0.2 0.4 1', 4, 6)
w.light('sun', 'ldirectional', 0, 6, 5, '1 0.8 0.5', 3, 30, (0.5, 0, -0.9))
w.light_config = lambda: {"point": 4, "spot": 0, "tube": 0, "directional": 1}
w.save()

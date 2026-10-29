# 22a. model-light-hidden: the light config has one point light, and the first of two is hidden: does it keep the slot?
w = Scene3D('model-light-hidden', 'a hidden light and a shown one, with one point light in the light config',
            ambient='0.05 0.05 0.05')
lit = w.material('lit', '1 1 1', lighting=True)
for i in range(3):
    w.cube('cube %d' % i, lit, -4 + i * 4, 0, 0, (0.3, 0.6, 0), '1.3 1.3 1.3')
w.light('hidden', 'lpoint', -3, 3, 4, '1 0.2 0.2', 6, 30, visible=False)
w.light('shown', 'lpoint', 3, 3, 4, '0.2 1 0.2', 6, 30)
w.light_config = lambda: {"point": 1, "spot": 0, "tube": 0, "directional": 0}
w.save()

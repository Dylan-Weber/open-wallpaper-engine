# model-shadow-oneside: two one-sided ceilings over a lit floor, one facing down (towards the floor, away from the sun)
# and one facing up (towards the sun): which of them cast a shadow on the floor? (Ocarina of Time's corridor is made of
# one-sided surfaces, and its floor has a sunlit patch owe's shadows cover)
import math
w = Scene3D('model-shadow-oneside', 'one-sided ceilings and the sun', ambient='0.05 0.05 0.05')
lit = w.material('lit', '1 1 1', lighting=True)
w.cube('floor', lit, 0, -1.6, 6, (0, 0, 0), '8 0.1 8')
for name, x, angle in (('down', -3, math.pi / 2), ('up', 3, -math.pi / 2)):
    mdl = w.material('ceiling-' + name, '0.8 0.8 0.8', lighting=True)
    write_binary(os.path.join(w.name, mdl), quad_mdl('materials/ceiling-%s.json' % name, 2.0))
    w.cube('ceiling ' + name, mdl, x, 1.2, 6, (angle, 0, 0))
w.light('sun', 'ldirectional', 0, 6, 5, '1 1 1', 3, 30, (0.5, 0, -0.9), castshadow=True)
w.light_config = lambda: {"point": 0, "spot": 0, "tube": 0, "directional": 1, "directionalshadow": 1}
w.save()

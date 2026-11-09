# shadow-oneside-wall: one-sided walls standing on a floor (Ocarina of Time's courtyard: the lit strip at a one-sided
# wall's foot, where light leaks under the wall's own shadow, is wider in WE). shadow-contact's view and OoT's sun and
# cascades, medium shadows; two one-sided walls 2 x 1 standing on the floor, one facing the sun's side (+x), one
# facing away (-x), and a closed box wall for comparison.
import math
w = Scene3D('shadow-oneside-wall', 'one-sided walls on a floor', ambient='0.15 0.15 0.15', eye='6 2.5 6')
lit = w.material('lit', '1 1 1', lighting=True)
w.cube('floor', lit, 0, -0.1, 0, (0, 0, 0), '10 0.1 10')
for name, z, yaw in (('east', 2.5, math.pi / 2), ('west', 0, -math.pi / 2)):
    mdl = w.material('wall-' + name, '0.8 0.8 0.8', lighting=True)
    write_binary(os.path.join(w.name, mdl), quad_mdl('materials/wall-%s.json' % name, 1.0))
    w.cube('wall ' + name, mdl, -2, 1.0, z, (0, yaw, 0), '1.5 1 1')
w.cube('box wall', lit, -2, 1.0, -2.5, (0, 0, 0), '0.05 1.0 1.5')
w.light('sun', 'ldirectional', 0, 1.519, 0, '1 1 1', 2, 30, (0.0, 0.32301, -2.06849), castshadow=True,
        cascadedistance0=3.0, cascadedistance1=14.34, cascadedistance2=100.0)
w.light_config = lambda: {"point": 0, "spot": 0, "tube": 0, "directional": 1, "directionalshadow": 1}
w.save()
with open(os.path.join(HERE, w.name, 'compare.json'), 'w') as f:
    json.dump({"probe": True, "fps": 30, "shadow_quality": "medium"}, f)
clock_probe.stamp(os.path.join(HERE, w.name))

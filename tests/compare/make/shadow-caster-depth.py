# shadow-caster-depth: are casters far along the light (outside a cascade's depth range) clamped, cut, or dropped?
# (oot-repro's courtyard: owe's shadow edge from the tall, distant roof sits 3-5 px off WE's.) Ocarina of Time's camera
# (as in shadow-cascade-switch), sun and cascades (3 / 14.34 / 100), medium shadows; posts 1, 2, 4, 8, 16 and 32 units
# tall standing 3 units ahead on a wide floor: their shadows' lengths grow with their height, until a cascade's depth
# range cuts the top off.
w = Scene3D('shadow-caster-depth', 'tall casters and the cascades depth range', ambient='0.1 0.1 0.1',
            eye='0.988 0.139 0.057')
lit = w.material('lit', '1 1 1', lighting=True)
w.cube('floor', lit, -20, -1.406, 0, (0, 0, 0), '40 0.1 40')
for i, h in enumerate((1, 2, 4, 8, 16, 32)):
    w.cube('post %g' % h, lit, -2.0 - 0.3 * i, -1.306 + h / 2, 0.9 - 0.45 * i, (0, 0, 0), '0.04 %g 0.04' % (h / 2))
w.light('sun', 'ldirectional', 0, 0.2, 0, '1 1 1', 3.09, 5, (0.0, 0.32301, -2.06849), castshadow=True,
        cascadedistance0=3.0, cascadedistance1=14.34, cascadedistance2=100.0)
w.light_config = lambda: {"point": 0, "spot": 0, "tube": 0, "directional": 1, "directionalshadow": 1}
w.save()
with open(os.path.join(HERE, w.name, 'compare.json'), 'w') as f:
    json.dump({"probe": True, "fps": 30, "shadow_quality": "medium"}, f)
clock_probe.stamp(os.path.join(HERE, w.name))

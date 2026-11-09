# shadow-caster-behind: casters between the floor and the sun, far behind the camera: before a cascade's near plane,
# are they clamped (still casting) or clipped? (oot-repro's courtyard: the lit strip at a wall's foot ends at the shadow of
# the opposite wall, behind the camera, and is wider in WE.) Ocarina of Time's camera, sun and cascades, medium shadows;
# tall thin walls 5, 15, 30 and 45 units behind the camera (towards the sun), each over its own lane of the floor.
w = Scene3D('shadow-caster-behind', 'casters behind the camera, towards the sun', ambient='0.1 0.1 0.1',
            eye='0.988 0.139 0.057')
lit = w.material('lit', '1 1 1', lighting=True)
w.cube('floor', lit, 0, -1.406, 0, (0, 0, 0), '80 0.1 80')
for i, d in enumerate((5, 15, 30, 45)):
    w.cube('wall %g' % d, lit, 0.988 + d, -1.306 + 40, 6 - 4 * i, (0, 0, 0), '0.1 40 1.5')
w.light('sun', 'ldirectional', 0, 0.2, 0, '1 1 1', 3.09, 5, (0.0, 0.32301, -2.06849), castshadow=True,
        cascadedistance0=3.0, cascadedistance1=14.34, cascadedistance2=100.0)
w.light_config = lambda: {"point": 0, "spot": 0, "tube": 0, "directional": 1, "directionalshadow": 1}
w.save()
with open(os.path.join(HERE, w.name, 'compare.json'), 'w') as f:
    json.dump({"probe": True, "fps": 30, "shadow_quality": "medium"}, f)
clock_probe.stamp(os.path.join(HERE, w.name))

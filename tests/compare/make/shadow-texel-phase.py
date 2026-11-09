# shadow-texel-phase: where a directional light's shadow-map texels fall (oot-repro's sunlit floor patch: the stair steps
# of its shadow edge sit at another phase in owe). A lit wall at z = -1 faces the camera (12 units away); a thin bar in front
# of it, turned 30 degrees, casts a diagonal shadow edge from a sun shining almost along -z. The first cascade is 30 units
# wide, so at medium shadows (256-texel squares) a texel is 30/256 units, ~9 screen pixels: the edge is a staircase.
# Variants move the camera sideways by a fraction of a texel (the cascades follow the camera): -x1 by 0.039 units
# (a third of a texel), -x2 by 0.078. -fov25 and -oot: Ocarina of Time's field of view and cascades.
def make(name, eye_x, fov=50, cascades=(30.0, 60.0, 100.0)):
    w = Scene3D(name, 'shadow-map texel phase (camera x %g)' % eye_x, ambient='0.1 0.1 0.1', eye='%g 0 12' % eye_x)
    lit = w.material('lit', '1 1 1', lighting=True)
    w.cube('wall', lit, eye_x, 0, -1, (0, 0, 0), '12 7 0.1')
    w.cube('bar', lit, eye_x - 2, 0.5, 2, (0, 0, math.radians(30)), '2 6 0.2')
    w.light('sun', 'ldirectional', 0, 0, 10, '1 1 1', 1, 30, (0.1, math.radians(90) - 0.5, 0.05), castshadow=True,
            cascadedistance0=cascades[0], cascadedistance1=cascades[1], cascadedistance2=cascades[2])
    w.general['fov'] = fov
    w.light_config = lambda: {"point": 0, "spot": 0, "tube": 0, "directional": 1, "directionalshadow": 1}
    w.save()
    with open(os.path.join(HERE, name, 'compare.json'), 'w') as f:
        json.dump({"probe": True, "fps": 30}, f)
    clock_probe.stamp(os.path.join(HERE, name))


make('shadow-texel-phase', 0.0)
make('shadow-texel-phase-x1', 0.039)
make('shadow-texel-phase-x2', 0.078)
# Ocarina of Time's settings: a field of view of 25 (its script zooms 50 -> 25) and cascades of 3, 14.34 and 100
make('shadow-texel-phase-fov25', 0.0, fov=25)
make('shadow-texel-phase-oot', 0.0, cascades=(3.0, 14.34, 100.0))

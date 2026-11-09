# shadow-cascade-switch: which cascade a directional light's shadow comes from, at which distance (oot-repro's courtyard:
# owe's shadow edge on the floor is sharper and nearer the wall than WE's, as if owe used the first cascade where WE has
# gone to the second). Ocarina of Time's camera (eye 9.978 1.445 -0.045 looking at 8.990 1.306 -0.102, fov 50, moved so
# it looks at the origin), its sun and cascades (3 / 14.34 / 100) and medium shadows; thin posts on the floor at 1..14
# units ahead, a little to the left: each post's shadow edge is as soft as the texels of the cascade drawing it.
def make(name, side):
    w = Scene3D(name, 'which cascade draws the floor (posts %g a unit aside)' % side, ambient='0.1 0.1 0.1',
              eye='0.988 0.139 0.057')
    lit = w.material('lit', '1 1 1', lighting=True)
    w.cube('floor', lit, -10, -1.406, 0, (0, 0, 0), '14 0.1 12')
    for i, d in enumerate((1, 1.5, 2, 2.5, 3, 4, 5, 6, 8, 10, 12, 14)):
            w.cube('post %g' % d, lit, 0.988 - d, -0.806, 0.35 + side * d, (0, 0, 0), '0.03 0.5 0.03')
    w.light('sun', 'ldirectional', 0, 0.2, 0, '1 1 1', 3.09, 5, (0.0, 0.32301, -2.06849), castshadow=True,
          cascadedistance0=3.0, cascadedistance1=14.34, cascadedistance2=100.0)
    w.light_config = lambda: {"point": 0, "spot": 0, "tube": 0, "directional": 1, "directionalshadow": 1}
    w.save()
    with open(os.path.join(HERE, name, 'compare.json'), 'w') as f:
            json.dump({"probe": True, "fps": 30, "shadow_quality": "medium"}, f)
    clock_probe.stamp(os.path.join(HERE, name))


make('shadow-cascade-switch', 0.08)
# posts ~27 degrees left of the view, where oot-repro's wall meets its floor
make('shadow-cascade-switch-side', 0.5)

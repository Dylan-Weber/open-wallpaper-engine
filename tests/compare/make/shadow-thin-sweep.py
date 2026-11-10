# shadow-thin-sweep: casters thinner than a shadow texel (the courtyard stripe and shadow-cascade-switch-wide-first
# differ only where a caster is thinner than a texel or edge-on to the light; thick ones match). Seen from above, 16
# posts 0.03 wide, 0.5 tall, 0.45 apart along x plus a sixteenth of a texel each (one texel across the row), under
# OoT's sun and a first cascade 30 wide (medium: 30 / 256 = 0.117 a texel), the second 31 so the first draws them all.
# Each post's shadow is as dark as the texels its footprint covers. -edge: the same row of thin upright plates
# (0.5 x 0.5, 0.002 thick) turned along the light, edge-on.
TEXEL = 30.0 / 256


def make(name, plate=False):
    w = Scene3D(name, 'casters thinner than a shadow texel (%s)' % name, ambient='0.1 0.1 0.1', eye='0 6 2')
    lit = w.material('lit', '1 1 1', lighting=True)
    w.cube('floor', lit, 0, -0.6, 0, (0, 0, 0), '4 0.1 4')
    for i in range(16):
        x = -3.4 + 0.45 * i + TEXEL * i / 16
        w.cube('post %d' % i, lit, x, -0.25, 0.8, (0, 0, 0), '0.25 0.25 0.001' if plate else '0.015 0.25 0.015')
    w.light('sun', 'ldirectional', 0, 2, 0, '1 1 1', 3.09, 5, (0.0, 0.32301, -2.06849), castshadow=True,
            cascadedistance0=30.0, cascadedistance1=31.0, cascadedistance2=100.0)
    w.light_config = lambda: {"point": 0, "spot": 0, "tube": 0, "directional": 1, "directionalshadow": 1}
    w.save()
    with open(os.path.join(HERE, name, 'compare.json'), 'w') as f:
        json.dump({"probe": True, "fps": 30, "shadow_quality": "medium"}, f)
    clock_probe.stamp(os.path.join(HERE, name))


make('shadow-thin-sweep')
make('shadow-thin-sweep-edge', plate=True)

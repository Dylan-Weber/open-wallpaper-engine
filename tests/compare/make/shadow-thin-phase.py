# shadow-thin-phase: a phase scan of casters thinner than a shadow texel. 48 slats 0.03 wide (along x) and 1.2 long
# (along z), spaced 3 texels plus a 24th of a texel along x (texel = 30 / 256, first cascade 30 wide at medium), so each
# next slat meets the shadow texel grid a little further along. A slat's shadow shows only where its footprint covers a
# texel's centre, so the pattern of shadowed slats gives the cascade's texel pitch and phase as each engine draws it.
# -ax / -ay: the sun turned about z by -2 / -1 radians (its shadow square's axes are world z and the xy plane's);
# -bx: turned about x by -1 as well. (Wallpaper Engine's cascades are 1.0017 times wider than authored: its light axes
# come out of a fast inverse square root, 0.99831 long. Without that, a few slats' shadows differ.)
TEXEL = 30.0 / 256


def make(name, half=0.015, angles=(0.0, 0.32301, -2.06849)):
    w = Scene3D(name, 'thin post phase scan (%s)' % name, ambient='0.1 0.1 0.1', eye='0 12 2')
    lit = w.material('lit', '1 1 1', lighting=True)
    w.cube('floor', lit, 0, -0.6, 0, (0, 0, 0), '10 0.1 4')
    step = 3 * TEXEL + TEXEL / 24
    for i in range(48):
        x = -8.2 + step * i
        w.cube('post %d' % i, lit, x, -0.25, 0.0, (0, 0, 0), '%g 0.25 0.6' % half)
    w.light('sun', 'ldirectional', 0, 2, 0, '1 1 1', 3.09, 5, angles, castshadow=True,
            cascadedistance0=30.0, cascadedistance1=31.0, cascadedistance2=100.0)
    w.light_config = lambda: {"point": 0, "spot": 0, "tube": 0, "directional": 1, "directionalshadow": 1}
    w.save()
    with open(os.path.join(HERE, name, 'compare.json'), 'w') as f:
        json.dump({"probe": True, "fps": 30, "shadow_quality": "medium"}, f)
    clock_probe.stamp(os.path.join(HERE, name))


make('shadow-thin-phase-ax', angles=(0.0, 0.0, -2.0))
make('shadow-thin-phase-ay', angles=(0.0, 0.0, -1.0))
make('shadow-thin-phase-bx', angles=(-1.0, 0.0, -2.0))

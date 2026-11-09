# shadow-grazing: where a low sun's shadow edge falls on a floor (oot-repro: the sunlit strip along a wall's base is ~15 px
# wider in Wallpaper Engine; its far edge, cast from far away by a low sun, sits nearer the wall in owe). A floor, a wall
# 2 units tall across x at x = -6, and a sun at an elevation of 10, 20 or 40 degrees shining along +x (a directional light
# shines along its x axis: angles 0 0 -elevation): the wall's shadow ends 2 / tan(elevation) from its foot, give or take
# the shadow bias. OoT's cascades (3, 14.34, 100).
def make(name, elevation):
    w = Scene3D(name, 'low sun shadow edge (elevation %d)' % elevation, ambient='0.1 0.1 0.1', eye='0 6 14')
    w.general['fov'] = 50
    lit = w.material('lit', '1 1 1', lighting=True)
    w.cube('floor', lit, 0, -0.1, 0, (0, 0, 0), '20 0.1 20')
    w.cube('wall', lit, -6, 1, 0, (0, 0, 0), '0.2 1 20')
    w.light('sun', 'ldirectional', -10, 5, 0, '1 1 1', 3, 30, (0, 0, -math.radians(elevation)),
            castshadow=True, cascadedistance0=3.0, cascadedistance1=14.34, cascadedistance2=100.0)
    w.light_config = lambda: {"point": 0, "spot": 0, "tube": 0, "directional": 1, "directionalshadow": 1}
    w.save()
    with open(os.path.join(HERE, name, 'compare.json'), 'w') as f:
        json.dump({"probe": True, "fps": 30}, f)
    clock_probe.stamp(os.path.join(HERE, name))


for elevation in (10, 20, 40):
    make('shadow-grazing-%d' % elevation, elevation)

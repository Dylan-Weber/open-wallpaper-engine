# shadow-model-scale: does a model's scale change its shadow? (Ocarina of Time's courtyard is a model scaled 0.01 with
# vertices in the hundreds; its wall's shadow edge on the floor sits 3-5 px off WE's.) shadow-contact's wall standing
# on a floor, under OoT's sun and cascades: -1 built of unit cubes scaled to size, -100 of cubes 100 times larger
# scaled by 0.01 (the same world geometry).
def make(name, k):
    w = Scene3D(name, 'shadow of a model scaled 1/%g' % k, ambient='0.15 0.15 0.15', eye='6 2.5 6')
    mat = 'materials/lit.json'
    w.files[mat] = {"passes": [{"shader": "generic4", "combos": {"LIGHTING": 1},
                                "constantshadervalues": {"color": "1 1 1", "alpha": 1.0}, "textures": [None]}]}
    mdl = 'models/big.mdl'
    write_binary(os.path.join(w.name, mdl), cube_mdl(mat, size=float(k)))
    s = lambda x, y, z: '%g %g %g' % (x / k, y / k, z / k)
    w.cube('floor', mdl, 0, -0.1, 0, (0, 0, 0), s(10, 0.1, 10))
    w.cube('wall', mdl, -2, 1.0, 0, (0, 0, 0), s(0.3, 1.0, 6))
    w.cube('box', mdl, 2, 0.5, -2, (0, 0.6, 0), s(0.6, 0.5, 0.6))
    w.light('sun', 'ldirectional', 0, 1.519, 0, '1 1 1', 2, 30, (0.0, 0.32301, -2.06849), castshadow=True,
            cascadedistance0=3.0, cascadedistance1=14.34, cascadedistance2=100.0)
    w.light_config = lambda: {"point": 0, "spot": 0, "tube": 0, "directional": 1, "directionalshadow": 1}
    w.save()
    with open(os.path.join(HERE, name, 'compare.json'), 'w') as f:
        json.dump({"probe": True, "fps": 30, "shadow_quality": "medium"}, f)
    clock_probe.stamp(os.path.join(HERE, name))


make('shadow-model-scale-1', 1)
make('shadow-model-scale-100', 100)

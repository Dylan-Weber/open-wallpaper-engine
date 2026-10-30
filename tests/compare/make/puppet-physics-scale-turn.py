# puppet-physics-scale-turn: three strips whose tip bone is turned by gravity (rotation spring only: mass 100, stiffness 250), one as is, one
# with the object's scale 2, one with its size doubled: how far each turns (a still picture), which says whether the object's scale
# changes the gravity's turn (it halves a translation spring's sag: puppet-physics-scale).
w = Wallpaper('puppet-physics-scale-turn', 'gravity with the object scaled', clear='0 0 0')
vertices, indices = strip_mesh(bones=3)
one = (1, 1, 1)
rest = [[((0, 0, 0), (0, 0, 0), one)] * 2 for _ in range(3)]
base = {"a": "1 0 0", "gd": "0 -1 0", "ge": True, "ik": False, "ikce": False, "ikd": 2, "ikg": True, "ikr": True,
        "ikrd": 25, "ikrmax": 3.1415927, "ikrmin": 0, "la": False, "lamax": "3.14159 3.14159 3.14159",
        "lamin": "-3.14159 -3.14159 -3.14159", "layout": "advanced", "lt": False, "ltmax": 100, "m": 100,
        "preset": "none", "presetresponsiveness": 100, "r": True, "rax": True, "ray": True, "raz": True,
        "re": False, "rf": 35, "ri": -5, "rs": 250, "s": 0, "se": True, "t": False, "tax": True, "tay": True,
        "taz": True, "tf": 20, "ti": 30, "tm": 450.0, "tp": "62.5 0 0", "ts": 500}
for i, (title, scale, size, y) in enumerate((('as is', '1 1 1', '600 100', 800), ('scale 2', '2 2 1', '600 100', 520),
                                              ('size doubled', '1 1 1', '1200 200', 200))):
    write_binary('puppet-physics-scale-turn/models/strip%d_puppet.mdl' % i, puppet_mdl(
        'materials/strip.json', vertices, indices,
        [('root', None, (0, 0, 0)), ('mid', 0, (150, 0, 0)), ('tip', 1, (150, 0, 0), json.dumps(base))],
        [('idle', 30.0, rest)]))
    model = w.texture_model('strip%d' % i, 'gradient/gradient_rainbow',
                            model_extra={"puppet": "models/strip%d_puppet.mdl" % i})
    w.add({"name": "strip %d" % i, "image": model, "origin": "960 %d 0" % y, "size": size, "color": "1 1 1",
           "alpha": 1, "angles": "0 0 0", "scale": scale, "alignment": "center",
           "animationlayers": [{"animation": 1, "blend": 1.0, "rate": 1.0, "visible": True, "id": 1, "name": "idle"}]})
w.save()
with open(os.path.join(HERE, 'puppet-physics-scale-turn', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'puppet-physics-scale-turn'))

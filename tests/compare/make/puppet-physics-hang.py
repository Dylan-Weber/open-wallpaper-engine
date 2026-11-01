# puppet-physics-hang: six strips whose tip bone has only a rotation spring (the translation one off) under gravity: how far
# gravity turns it (a still picture) with the mass, the stiffness, the tip's length and gravity's direction changed from a
# base of mass 100, stiffness 250, tip 62.5 0 0, gravity 0 -1 0. owe's rotation spring ignores gravity; Wallpaper Engine's
# turns the tip bone (Gun Girl's hair, gun and hands hang like that).
w = Wallpaper('puppet-physics-hang', 'rotation under gravity: mass, stiffness, tip and direction', clear='0 0 0')
vertices, indices = strip_mesh(bones=3)
one = (1, 1, 1)
rest = [[((0, 0, 0), (0, 0, 0), one)] * 2 for _ in range(3)]
base = {"a": "1 0 0", "gd": "0 -1 0", "ge": True, "ik": False, "ikce": False, "ikd": 2, "ikg": True, "ikr": True,
        "ikrd": 25, "ikrmax": 3.1415927, "ikrmin": 0, "la": False, "lamax": "3.14159 3.14159 3.14159",
        "lamin": "-3.14159 -3.14159 -3.14159", "layout": "advanced", "lt": False, "ltmax": 100, "m": 100,
        "preset": "none", "presetresponsiveness": 100, "r": True, "rax": True, "ray": True, "raz": True,
        "re": False, "rf": 35, "ri": -5, "rs": 250, "s": 0, "se": True, "t": False, "tax": True, "tay": True,
        "taz": True, "tf": 50, "ti": 5, "tm": 450.0, "tp": "62.5 0 0", "ts": 500}
variants = (({}, 900), ({"m": 200}, 760), ({"rs": 500}, 620), ({"tp": "125 0 0"}, 480), ({"gd": "1 0 0"}, 340),
            ({"gd": "0 1 0"}, 200))
for i, (change, y) in enumerate(variants):
    write_binary('puppet-physics-hang/models/strip%d_puppet.mdl' % i, puppet_mdl(
        'materials/strip.json', vertices, indices,
        [('root', None, (0, 0, 0)), ('mid', 0, (150, 0, 0)), ('tip', 1, (150, 0, 0), json.dumps({**base, **change}))],
        [('idle', 30.0, rest)]))
    model = w.texture_model('strip%d' % i, 'gradient/gradient_rainbow',
                            model_extra={"puppet": "models/strip%d_puppet.mdl" % i})
    w.add({"name": "strip %d" % i, "image": model, "origin": "960 %d 0" % y, "size": "600 100", "color": "1 1 1",
           "alpha": 1, "angles": "0 0 0", "scale": "1 1 1", "alignment": "center",
           "animationlayers": [{"animation": 1, "blend": 1.0, "rate": 1.0, "visible": True, "id": 1, "name": "idle"}]})
w.save()
with open(os.path.join(HERE, 'puppet-physics-hang', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'puppet-physics-hang'))

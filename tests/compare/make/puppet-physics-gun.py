# puppet-physics-gun: strips whose tip bone has the physics of Gun Girl's gun and hands (mass 100, translation spring 50, rotation spring -5
# (negative), inertia -10, tip 1000 0 0, rotation about z only, translation axes x and y locked), as in the file and with one value changed
# at a time: where each tip settles under gravity (a still picture).
w = Wallpaper('puppet-physics-gun', 'a real puppet bone: negative rotation spring, long tip', clear='0 0 0')
vertices, indices = strip_mesh(bones=3)
one = (1, 1, 1)
rest = [[((0, 0, 0), (0, 0, 0), one)] * 2 for _ in range(3)]
real = {"a": "1 0 0", "gd": "0 -1 0", "ge": True, "ik": False, "ikce": False, "ikd": 2, "ikg": True, "ikr": True,
        "ikrd": 25, "ikrmax": 3.1415927, "ikrmin": 0, "la": False, "lamax": "3.14159 3.14159 3.14159",
        "lamin": "-3.14159 -3.14159 -3.14159", "layout": "advanced", "lt": False, "ltmax": 100, "m": 100,
        "preset": "none", "presetresponsiveness": 100, "r": True, "rax": False, "ray": False, "raz": True,
        "re": False, "rf": 25, "ri": -10, "rs": -5, "s": 1000, "se": True, "t": True, "tax": False, "tay": False,
        "taz": True, "tf": 30, "ti": 5, "tm": 500.0, "tp": "1000 0 0", "ts": 50}
variants = (('as in the file', 900, {}), ('rotation only', 760, {"t": False}), ('positive rotation spring', 620, {"rs": 5}),
            ('positive inertia', 480, {"ri": 10}), ('short tip', 340, {"tp": "62.5 0 0"}),
            ('short tip, rotation only', 200, {"tp": "62.5 0 0", "t": False}))
for i, (title, y, change) in enumerate(variants):
    write_binary('puppet-physics-gun/models/strip%d_puppet.mdl' % i, puppet_mdl(
        'materials/strip.json', vertices, indices,
        [('root', None, (0, 0, 0)), ('mid', 0, (150, 0, 0)), ('tip', 1, (150, 0, 0), json.dumps({**real, **change}))],
        [('idle', 30.0, rest)]))
    model = w.texture_model('strip%d' % i, 'gradient/gradient_rainbow',
                            model_extra={"puppet": "models/strip%d_puppet.mdl" % i})
    w.add({"name": "strip %d" % i, "image": model, "origin": "960 %d 0" % y, "size": "600 100", "color": "1 1 1",
           "alpha": 1, "angles": "0 0 0", "scale": "1 1 1", "alignment": "center",
           "animationlayers": [{"animation": 1, "blend": 1.0, "rate": 1.0, "visible": True, "id": 1, "name": "idle"}]})
w.save()
with open(os.path.join(HERE, 'puppet-physics-gun', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'puppet-physics-gun'))

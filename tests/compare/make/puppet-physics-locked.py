# puppet-physics-locked: three strips whose tip has a real puppet's physics (Gun Girl's: translation and rotation springs under
# gravity, mass 100, stiffness 500, translation axes x and y locked), with the axes locked as in the file, with them free, and
# with only the rotation spring: where each tip settles under gravity (a still picture).
w = Wallpaper('puppet-physics-locked', 'a real puppet bone: axes locked, gravity', clear='0 0 0')
vertices, indices = strip_mesh(bones=3)
one = (1, 1, 1)
rest = [[((0, 0, 0), (0, 0, 0), one)] * 2 for _ in range(3)]
real = {"a": "1 0 0", "gd": "0 -1 0", "ge": True, "ik": False, "ikce": False, "ikd": 2, "ikg": True, "ikr": True,
        "ikrd": 25, "ikrmax": 3.1415927, "ikrmin": 0, "la": False, "lamax": "3.14159 3.14159 3.14159",
        "lamin": "-3.14159 -3.14159 -3.14159", "layout": "advanced", "lt": False, "ltmax": 100, "m": 100,
        "preset": "none", "presetresponsiveness": 100, "r": True, "rax": True, "ray": True, "raz": True,
        "re": False, "rf": 35, "ri": -5, "rs": 250, "s": 0, "se": True, "t": True, "tax": False, "tay": False,
        "taz": True, "tf": 50, "ti": 5, "tm": 450.0, "tp": "62.5 0 0", "ts": 500}
variants = (('locked axes (as in the file)', 800, {}), ('free axes', 540, {"tax": True, "tay": True}),
            ('rotation only', 280, {"t": False}))
for i, (title, y, change) in enumerate(variants):
    write_binary('puppet-physics-locked/models/strip%d_puppet.mdl' % i, puppet_mdl(
        'materials/strip.json', vertices, indices,
        [('root', None, (0, 0, 0)), ('mid', 0, (150, 0, 0)), ('tip', 1, (150, 0, 0), json.dumps({**real, **change}))],
        [('idle', 30.0, rest)]))
    model = w.texture_model('strip%d' % i, 'gradient/gradient_rainbow',
                            model_extra={"puppet": "models/strip%d_puppet.mdl" % i})
    w.add({"name": "strip %d" % i, "image": model, "origin": "960 %d 0" % y, "size": "600 100", "color": "1 1 1",
           "alpha": 1, "angles": "0 0 0", "scale": "1 1 1", "alignment": "center",
           "animationlayers": [{"animation": 1, "blend": 1.0, "rate": 1.0, "visible": True, "id": 1, "name": "idle"}]})
w.save()
with open(os.path.join(HERE, 'puppet-physics-locked', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'puppet-physics-locked'))

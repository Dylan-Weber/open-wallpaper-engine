# puppet-physics-gunsag: strips whose tip hangs under gravity with the physics of Gun Girl's gun (mass 25, translation spring 1000, friction 150,
# inertia -50, largest distance 100, rotation spring -5, all translation axes free, rotation limited to 90 degrees) and with parts of it
# changed: how far each tip sags (a still picture): Wallpaper Engine's gun sags to the largest distance where the numbers say 25.
w = Wallpaper('puppet-physics-gunsag', 'the gun bone: sag under gravity', clear='0 0 0')
vertices, indices = strip_mesh(bones=3)
one = (1, 1, 1)
rest = [[((0, 0, 0), (0, 0, 0), one)] * 2 for _ in range(3)]
gun = {"a": "1 0 0", "gd": "0 -1 0", "ge": True, "ik": False, "ikce": False, "ikd": 2, "ikg": True, "ikr": True,
       "ikrd": 25, "ikrmax": 3.1415927, "ikrmin": 0, "la": True, "lamax": "0.00000 0.00000 1.57080",
       "lamin": "0.00000 0.00000 -1.57080", "layout": "advanced", "lt": False, "ltmax": 180, "m": 25,
       "preset": "none", "presetresponsiveness": 100, "r": True, "rax": False, "ray": False, "raz": True,
       "re": False, "rf": 25, "ri": -5, "rs": -5, "s": 1000, "se": True, "t": True, "tax": True, "tay": True,
       "taz": True, "tf": 150, "ti": -50, "tm": 100.0, "tp": "1000 0 0", "ts": 1000}
variants = (('the gun', 900, {}), ('rotation off', 760, {"r": False}), ('inertia 30', 620, {"ti": 30}),
            ('friction 20', 480, {"tf": 20}), ('short tip, rotation off', 340, {"r": False, "tp": "62.5 0 0"}),
            ('mass 100', 200, {"m": 100}))
for i, (title, y, change) in enumerate(variants):
    write_binary('puppet-physics-gunsag/models/strip%d_puppet.mdl' % i, puppet_mdl(
        'materials/strip.json', vertices, indices,
        [('root', None, (0, 0, 0)), ('mid', 0, (150, 0, 0)), ('tip', 1, (150, 0, 0), json.dumps({**gun, **change}))],
        [('idle', 30.0, rest)]))
    model = w.texture_model('strip%d' % i, 'gradient/gradient_rainbow',
                            model_extra={"puppet": "models/strip%d_puppet.mdl" % i})
    w.add({"name": "strip %d" % i, "image": model, "origin": "960 %d 0" % y, "size": "600 100", "color": "1 1 1",
           "alpha": 1, "angles": "0 0 0", "scale": "1 1 1", "alignment": "center",
           "animationlayers": [{"animation": 1, "blend": 1.0, "rate": 1.0, "visible": True, "id": 1, "name": "idle"}]})
w.save()
with open(os.path.join(HERE, 'puppet-physics-gunsag', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'puppet-physics-gunsag'))

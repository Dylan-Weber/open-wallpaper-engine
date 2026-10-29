# puppet-physics-gravity: three strips whose tip moves on a spring under gravity (ge, direction 0 -1 0), with masses 1, 2 and 4:
# each tip sags to where its spring balances gravity, and a still picture shows how far (owe adds mass x dt x 1000 x the
# direction to the velocity a frame, which this tests: the sag is 5, 10 and 20 units for a spring of 200 if so).
w = Wallpaper('puppet-physics-gravity', 'a tip hanging under gravity, three masses', clear='0 0 0')
vertices, indices = strip_mesh(bones=3)
one = (1, 1, 1)
rest = [[((0, 0, 0), (0, 0, 0), one)] * 2 for _ in range(3)]
base = {"a": "1 0 0", "gd": "0 -1 0", "ge": True, "ik": False, "ikce": False, "ikd": 2, "ikg": True, "ikr": True,
        "ikrd": 25, "ikrmax": 3.1415927, "ikrmin": 0, "la": False, "lamax": "3.14159 3.14159 3.14159",
        "lamin": "-3.14159 -3.14159 -3.14159", "layout": "advanced", "lt": False, "ltmax": 100, "m": 20,
        "preset": "none", "presetresponsiveness": 100, "r": False, "rax": True, "ray": True, "raz": True,
        "re": False, "rf": 20, "ri": 30, "rs": 200, "s": 0, "se": True, "t": True, "tax": True, "tay": True,
        "taz": True, "tf": 20, "ti": 30, "tm": 60.0, "tp": "62.5 0 0", "ts": 200}
for i, (mass, y) in enumerate(((1, 800), (2, 540), (4, 280))):
    write_binary('puppet-physics-gravity/models/strip%d_puppet.mdl' % i, puppet_mdl(
        'materials/strip.json', vertices, indices,
        [('root', None, (0, 0, 0)), ('mid', 0, (150, 0, 0)), ('tip', 1, (150, 0, 0), json.dumps({**base, "m": mass}))],
        [('idle', 30.0, rest)]))
    model = w.texture_model('strip%d' % i, 'gradient/gradient_rainbow',
                            model_extra={"puppet": "models/strip%d_puppet.mdl" % i})
    w.add({"name": "strip %d" % i, "image": model, "origin": "960 %d 0" % y, "size": "600 100", "color": "1 1 1",
           "alpha": 1, "angles": "0 0 0", "scale": "1 1 1", "alignment": "center",
           "animationlayers": [{"animation": 1, "blend": 1.0, "rate": 1.0, "visible": True, "id": 1, "name": "idle"}]})
w.save()
with open(os.path.join(HERE, 'puppet-physics-gravity', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'puppet-physics-gravity'))

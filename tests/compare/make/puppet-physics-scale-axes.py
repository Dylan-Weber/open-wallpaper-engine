# puppet-physics-scale-axes: three strips whose tip hangs on a spring under gravity (mass 20, spring 500: a sag of 40 model units), with the object
# scaled along z only, x only and y only, and one as is: how far each sags (a still picture), which says which of the scale's
# components Wallpaper Engine averages (its divisor is 3; a strip scaled 2 2 1 sags as one scaled 2).
w = Wallpaper('puppet-physics-scale-axes', 'gravity with the object scaled', clear='0 0 0')
vertices, indices = strip_mesh(bones=3)
one = (1, 1, 1)
rest = [[((0, 0, 0), (0, 0, 0), one)] * 2 for _ in range(3)]
base = {"a": "1 0 0", "gd": "0 -1 0", "ge": True, "ik": False, "ikce": False, "ikd": 2, "ikg": True, "ikr": True,
        "ikrd": 25, "ikrmax": 3.1415927, "ikrmin": 0, "la": False, "lamax": "3.14159 3.14159 3.14159",
        "lamin": "-3.14159 -3.14159 -3.14159", "layout": "advanced", "lt": False, "ltmax": 100, "m": 20,
        "preset": "none", "presetresponsiveness": 100, "r": False, "rax": True, "ray": True, "raz": True,
        "re": False, "rf": 20, "ri": 30, "rs": 200, "s": 0, "se": True, "t": True, "tax": True, "tay": True,
        "taz": True, "tf": 20, "ti": 30, "tm": 450.0, "tp": "62.5 0 0", "ts": 500}
for i, (title, scale, size, y) in enumerate((('z scaled 4', '1 1 4', '600 100', 880), ('x scaled 2', '2 1 1', '600 100', 680),
                                              ('y scaled 2', '1 2 1', '600 100', 460), ('as is', '1 1 1', '600 100', 240))):
    write_binary('puppet-physics-scale-axes/models/strip%d_puppet.mdl' % i, puppet_mdl(
        'materials/strip.json', vertices, indices,
        [('root', None, (0, 0, 0)), ('mid', 0, (150, 0, 0)), ('tip', 1, (150, 0, 0), json.dumps(base))],
        [('idle', 30.0, rest)]))
    model = w.texture_model('strip%d' % i, 'gradient/gradient_rainbow',
                            model_extra={"puppet": "models/strip%d_puppet.mdl" % i})
    w.add({"name": "strip %d" % i, "image": model, "origin": "960 %d 0" % y, "size": size, "color": "1 1 1",
           "alpha": 1, "angles": "0 0 0", "scale": scale, "alignment": "center",
           "animationlayers": [{"animation": 1, "blend": 1.0, "rate": 1.0, "visible": True, "id": 1, "name": "idle"}]})
w.save()
with open(os.path.join(HERE, 'puppet-physics-scale-axes', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'puppet-physics-scale-axes'))

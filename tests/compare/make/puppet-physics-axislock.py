# puppet-physics-axislock: strips whose tip bone hangs on a translation spring under gravity (mass 4: 20 units at rest), its
# pose turned 45 degrees from its parent, with its y axis locked (tay false), its x locked, or none. Wallpaper Engine takes
# the locked axes as the bone's own (as animated), so with y locked the tip sags along its own x, slantwise.
w = Wallpaper('puppet-physics-axislock', 'a hanging tip with an axis locked, its pose turned', clear='0 0 0')
vertices, indices = strip_mesh(bones=3)
one = (1, 1, 1)
turned = [[((0, 0, 0), (0, 0, 0), one)] * 2, [((0, 0, 0), (0, 0, 0), one)] * 2, [((0, 0, 0), (0, 0, 0.785398), one)] * 2]
base = {"a": "1 0 0", "gd": "0 -1 0", "ge": True, "ik": False, "ikce": False, "ikd": 2, "ikg": True, "ikr": True,
        "ikrd": 25, "ikrmax": 3.1415927, "ikrmin": 0, "la": False, "lamax": "3.14159 3.14159 3.14159",
        "lamin": "-3.14159 -3.14159 -3.14159", "layout": "advanced", "lt": False, "ltmax": 100, "m": 4,
        "preset": "none", "presetresponsiveness": 100, "r": False, "rax": True, "ray": True, "raz": True,
        "re": False, "rf": 20, "ri": 30, "rs": 200, "s": 0, "se": True, "t": True, "tax": True, "tay": True,
        "taz": True, "tf": 20, "ti": 30, "tm": 60.0, "tp": "62.5 0 0", "ts": 200}
for i, (change, y) in enumerate((({}, 920), ({"tay": False}, 680), ({"tax": False}, 440))):
    write_binary('puppet-physics-axislock/models/strip%d_puppet.mdl' % i, puppet_mdl(
        'materials/strip.json', vertices, indices,
        [('root', None, (0, 0, 0)), ('mid', 0, (150, 0, 0)), ('tip', 1, (150, 0, 0), json.dumps({**base, **change}))],
        [('idle', 30.0, turned)]))
    model = w.texture_model('strip%d' % i, 'gradient/gradient_rainbow',
                            model_extra={"puppet": "models/strip%d_puppet.mdl" % i})
    w.add({"name": "strip %d" % i, "image": model, "origin": "960 %d 0" % y, "size": "600 100", "color": "1 1 1",
           "alpha": 1, "angles": "0 0 0", "scale": "1 1 1", "alignment": "center",
           "animationlayers": [{"animation": 1, "blend": 1.0, "rate": 1.0, "visible": True, "id": 1, "name": "idle"}]})
w.save()
with open(os.path.join(HERE, 'puppet-physics-axislock', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'puppet-physics-axislock'))

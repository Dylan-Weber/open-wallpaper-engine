# puppet-physics-turned: strips whose tip bone has its own frame turned a quarter (as Gun Girl's gun bone has) and the gun's physics (gravity
# 0 1 0 along its tip 1000 0 0, translation inertia -50, rotation limited to 90 degrees), with the mass 2, 6 and 25 and one with an
# unturned frame: where each tip settles (a still picture). Wallpaper Engine's gun jumps to the translation limit, up for mass 2 and down for 6.
w = Wallpaper('puppet-physics-turned', 'the gun bone with a turned frame', clear='0 0 0')
import math
vertices, indices = strip_mesh(bones=3)
one = (1, 1, 1)
gun = {"a": "1 0 0", "gd": "0 1 0", "ge": True, "ik": False, "ikce": False, "ikd": 2, "ikg": True, "ikr": True,
       "ikrd": 25, "ikrmax": 3.1415927, "ikrmin": 0, "la": True, "lamax": "0.00000 0.00000 1.57080",
       "lamin": "0.00000 0.00000 -1.57080", "layout": "advanced", "lt": False, "ltmax": 180, "m": 25,
       "preset": "none", "presetresponsiveness": 100, "r": True, "rax": False, "ray": False, "raz": True,
       "re": False, "rf": 25, "ri": -5, "rs": -5, "s": 1000, "se": True, "t": True, "tax": True, "tay": True,
       "taz": True, "tf": 150, "ti": -50, "tm": 100.0, "tp": "1000 0 0", "ts": 1000}
variants = (('mass 2', 800, 90, {"m": 2}), ('mass 6', 640, 90, {"m": 6}), ('mass 25', 480, 90, {"m": 25}),
            ('unturned, mass 25', 320, 0, {"m": 25}))
import math
for i, (title, y, angle, change) in enumerate(variants):
    rest = [[((0, 0, 0), (0, 0, 0), one)] * 2, [((0, 0, 0), (0, 0, 0), one)] * 2,
            [((0, 0, 0), (0, 0, math.radians(angle)), one)] * 2]
    write_binary('puppet-physics-turned/models/strip%d_puppet.mdl' % i, puppet_mdl(
        'materials/strip.json', vertices, indices,
        [('root', None, (0, 0, 0)), ('mid', 0, (150, 0, 0)), ('tip', 1, (150, 0, 0, math.radians(angle)), json.dumps({**gun, **change}))],
        [('idle', 30.0, rest)]))
    model = w.texture_model('strip%d' % i, 'gradient/gradient_rainbow',
                            model_extra={"puppet": "models/strip%d_puppet.mdl" % i})
    w.add({"name": "strip %d" % i, "image": model, "origin": "960 %d 0" % y, "size": "600 100", "color": "1 1 1",
           "alpha": 1, "angles": "0 0 0", "scale": "1 1 1", "alignment": "center",
           "animationlayers": [{"animation": 1, "blend": 1.0, "rate": 1.0, "visible": True, "id": 1, "name": "idle"}]})
w.save()
with open(os.path.join(HERE, 'puppet-physics-turned', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'puppet-physics-turned'))

# puppet-physics-frame: strips whose bones have turned frames (as Gun Girl's do: her bones' frames are turned a quarter), tip bone with the
# physics of a real puppet bone (mass 100, rotation spring 250, tip 62.5 0 0 in the bone's own frame, gravity 0 -1 0): the root bone's frame is
# turned by 0, 90, -90, 180 and 45 degrees and the bones are placed so the strip is the same, where the tip settles (a still picture).
w = Wallpaper('puppet-physics-frame', 'rotation under gravity with turned bone frames', clear='0 0 0')
vertices, indices = strip_mesh(bones=3)
one = (1, 1, 1)
rest = [[((0, 0, 0), (0, 0, 0), one)] * 2 for _ in range(3)]
real = {"a": "1 0 0", "gd": "0 -1 0", "ge": True, "ik": False, "ikce": False, "ikd": 2, "ikg": True, "ikr": True,
        "ikrd": 25, "ikrmax": 3.1415927, "ikrmin": 0, "la": False, "lamax": "3.14159 3.14159 3.14159",
        "lamin": "-3.14159 -3.14159 -3.14159", "layout": "advanced", "lt": False, "ltmax": 100, "m": 100,
        "preset": "none", "presetresponsiveness": 100, "r": True, "rax": True, "ray": True, "raz": True,
        "re": False, "rf": 35, "ri": -5, "rs": 250, "s": 0, "se": True, "t": False, "tax": True, "tay": True,
        "taz": True, "tf": 50, "ti": 5, "tm": 450.0, "tp": "62.5 0 0", "ts": 500}
for i, (angle, y) in enumerate(((0, 880), (90, 740), (-90, 600), (180, 460), (45, 320))):
    a = math.radians(angle)
    c, s = math.cos(a), math.sin(a)
    step = (150 * c, -150 * s, 0)  # 150 along the strip, in the turned frame
    write_binary('puppet-physics-frame/models/strip%d_puppet.mdl' % i, puppet_mdl(
        'materials/strip.json', vertices, indices,
        [('root', None, (0, 0, 0, a)), ('mid', 0, step), ('tip', 1, step, json.dumps(real))],
        [('idle', 30.0, rest)]))
    model = w.texture_model('strip%d' % i, 'gradient/gradient_rainbow',
                            model_extra={"puppet": "models/strip%d_puppet.mdl" % i})
    w.add({"name": "strip %d" % i, "image": model, "origin": "960 %d 0" % y, "size": "600 100", "color": "1 1 1",
           "alpha": 1, "angles": "0 0 0", "scale": "1 1 1", "alignment": "center",
           "animationlayers": [{"animation": 1, "blend": 1.0, "rate": 1.0, "visible": True, "id": 1, "name": "idle"}]})
w.save()
with open(os.path.join(HERE, 'puppet-physics-frame', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'puppet-physics-frame'))

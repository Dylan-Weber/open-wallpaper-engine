# puppet-physics-scale-turn-axes: four strips whose tip bone is turned by gravity (rotation spring only: mass 100, stiffness
# 250, as in puppet-physics-scale-turn), with the object scaled unevenly: 1 1 1, 1 2 1, 2 1 1 and 1.5 0.75 1. Wallpaper
# Engine pulls the tip in the bone's own space, gravity divided by the object's scale axis by axis and multiplied by its mean
# scale, so a strip scaled along y is turned less and one scaled along x more.
w = Wallpaper('puppet-physics-scale-turn-axes', 'gravity on a turning bone, the object scaled unevenly', clear='0 0 0')
vertices, indices = strip_mesh(bones=3)
rest = [[((0, 0, 0), (0, 0, 0), (1, 1, 1))] * 2 for _ in range(3)]
base = {"a": "1 0 0", "gd": "0 -1 0", "ge": True, "ik": False, "ikce": False, "ikd": 2, "ikg": True, "ikr": True,
        "ikrd": 25, "ikrmax": 3.1415927, "ikrmin": 0, "la": False, "lamax": "3.14159 3.14159 3.14159",
        "lamin": "-3.14159 -3.14159 -3.14159", "layout": "advanced", "lt": False, "ltmax": 100, "m": 100,
        "preset": "none", "presetresponsiveness": 100, "r": True, "rax": True, "ray": True, "raz": True,
        "re": False, "rf": 35, "ri": -5, "rs": 250, "s": 0, "se": True, "t": False, "tax": True, "tay": True,
        "taz": True, "tf": 20, "ti": 30, "tm": 450.0, "tp": "62.5 0 0", "ts": 500}
write_binary('puppet-physics-scale-turn-axes/models/strip_puppet.mdl', puppet_mdl(
    'materials/strip.json', vertices, indices,
    [('root', None, (0, 0, 0)), ('mid', 0, (150, 0, 0)), ('tip', 1, (150, 0, 0), json.dumps(base))],
    [('idle', 30.0, rest)]))
model = w.texture_model('strip', 'gradient/gradient_rainbow', model_extra={"puppet": "models/strip_puppet.mdl"})
for i, (scale, x, y) in enumerate((('1 1 1', 560, 820), ('1 2 1', 560, 380), ('2 1 1', 1260, 820), ('1.5 0.75 1', 1260, 380))):
    w.add({"name": "strip %d" % i, "image": model, "origin": "%d %d 0" % (x, y), "size": "300 50", "color": "1 1 1",
           "alpha": 1, "angles": "0 0 0", "scale": scale, "alignment": "center",
           "animationlayers": [{"animation": 1, "blend": 1.0, "rate": 1.0, "visible": True, "id": 1, "name": "idle"}]})
w.save()
with open(os.path.join(HERE, 'puppet-physics-scale-turn-axes', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'puppet-physics-scale-turn-axes'))

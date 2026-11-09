# puppet-physics-turn-phase: ten strips like puppet-physics-turn's (the middle bone on a weakly damped rotation spring),
# each kicked once by a script, the first at 11 s of the scripts' clock and each next one a thirtieth of a second later.
# One still picture then shows the same swing at ten moments after its kick, with no frame-timing noise between them:
# when Wallpaper Engine's swing starts after the kick, and how large it is.
w = Wallpaper('puppet-physics-turn-phase', 'ten turning springs kicked a frame apart', clear='0 0 0')
vertices, indices = strip_mesh(height=50.0, bones=3)
one = (1, 1, 1)
rest = [[((0, 0, 0), (0, 0, 0), one)] * 2 for _ in range(3)]
PHYSICS = {"a": "1 0 0", "gd": "0 -1 0", "ge": False, "ik": False, "ikce": False, "ikd": 2, "ikg": True, "ikr": True,
           "ikrd": 25, "ikrmax": 3.1415927, "ikrmin": 0, "la": False, "lamax": "3.14159 3.14159 3.14159",
           "lamin": "-3.14159 -3.14159 -3.14159", "layout": "advanced", "lt": False, "ltmax": 100, "m": 20,
           "preset": "none", "presetresponsiveness": 100, "r": True, "rax": True, "ray": True, "raz": True,
           "re": False, "rf": 3, "ri": 30, "rs": 200, "s": 0, "se": True, "t": False, "tax": True, "tay": True,
           "taz": True, "tf": 3, "ti": 30, "tm": 60.0, "tp": "62.5 0 0", "ts": 200}
write_binary('puppet-physics-turn-phase/models/strip_puppet.mdl', puppet_mdl(
    'materials/strip.json', vertices, indices,
    [('root', None, (0, 0, 0)), ('mid', 0, (150, 0, 0), json.dumps(PHYSICS)), ('tip', 1, (150, 0, 0))],
    [('idle', 30.0, rest)]))
model = w.texture_model('strip', 'gradient/gradient_rainbow', model_extra={"puppet": "models/strip_puppet.mdl"})
for i in range(10):
    script = """'use strict';
let kicked = false;

export function update(value) {
    if (!kicked && engine.runtime >= %.6f) {
        kicked = true;
        thisLayer.applyBonePhysicsImpulse('mid', new Vec3(0, 0, 0), new Vec3(0, 0, 3));
    }
    return value;
}
""" % (11.0 + i / 30.0)
    w.add({"name": "strip %d" % i, "image": model,
           "origin": {"value": "960 %d 0" % (1020 - i * 80), "script": script, "scriptproperties": {}},
           "size": "600 50", "color": "1 1 1", "alpha": 1, "angles": "0 0 0", "scale": "1 1 1", "alignment": "center",
           "animationlayers": [{"animation": 1, "blend": 1.0, "rate": 1.0, "visible": True, "id": 1, "name": "idle"}]})
w.save()
with open(os.path.join(HERE, 'puppet-physics-turn-phase', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'puppet-physics-turn-phase'))

# puppet-physics-impulse: a strip of three bones whose middle bone turns on a spring and whose tip moves on a spring; a script
# kicks them alternately every 4 seconds (the middle bone's turn, then the tip's movement) and a series of pictures taken
# one after another follows how Wallpaper Engine's physics swings back (compare.json "shots"): each picture is compared at the
# frame its clock probe records, so the series shows how owe's spring physics follows over time.
w = Wallpaper('puppet-physics-impulse', 'bone physics kicked by a script', clear='0 0 0')
vertices, indices = strip_mesh(bones=3)
one = (1, 1, 1)
rest = [[((0, 0, 0), (0, 0, 0), one)] * 2 for _ in range(3)]
PHYSICS = {"a": "1 0 0", "gd": "0 -1 0", "ge": False, "ik": False, "ikce": False, "ikd": 2, "ikg": True, "ikr": True,
           "ikrd": 25, "ikrmax": 3.1415927, "ikrmin": 0, "la": False, "lamax": "3.14159 3.14159 3.14159",
           "lamin": "-3.14159 -3.14159 -3.14159", "layout": "advanced", "lt": False, "ltmax": 100, "m": 20,
           "preset": "none", "presetresponsiveness": 100, "r": False, "rax": True, "ray": True, "raz": True,
           "re": False, "rf": 20, "ri": 30, "rs": 200, "s": 0, "se": True, "t": False, "tax": True, "tay": True,
           "taz": True, "tf": 20, "ti": 30, "tm": 60.0, "tp": "62.5 0 0", "ts": 200}
# the physics of a bone as Wallpaper Engine writes it (every key)
turn = json.dumps({**PHYSICS, "r": True})
move = json.dumps({**PHYSICS, "t": True})
write_binary('puppet-physics-impulse/models/strip_puppet.mdl', puppet_mdl(
    'materials/strip.json', vertices, indices,
    [('root', None, (0, 0, 0)), ('mid', 0, (150, 0, 0), turn), ('tip', 1, (150, 0, 0), move)],
    [('idle', 30.0, rest)]))
model = w.texture_model('strip', 'gradient/gradient_rainbow', model_extra={"puppet": "models/strip_puppet.mdl"})
script = """'use strict';
let last = -1;

export function update(value) {
    const kick = Math.floor(engine.runtime / 4);
    if (kick !== last) {
        last = kick;
        if (kick % 2 === 0) thisLayer.applyBonePhysicsImpulse('mid', new Vec3(0, 0, 0), new Vec3(0, 0, 90));
        else thisLayer.applyBonePhysicsImpulse('tip', new Vec3(0, 600, 0), new Vec3(0, 0, 0));
    }
    return value;
}
"""
w.add({"name": "strip", "image": model, "origin": {"value": "960 540 0", "script": script, "scriptproperties": {}},
       "size": "600 100", "color": "1 1 1", "alpha": 1, "angles": "0 0 0", "scale": "1 1 1", "alignment": "center",
       "animationlayers": [{"animation": 1, "blend": 1.0, "rate": 1.0, "visible": True, "id": 1, "name": "idle"}]})
w.save()
with open(os.path.join(HERE, 'puppet-physics-impulse', 'compare.json'), 'w') as f:
    json.dump({"shots": 90, "shot_interval_ms": 0, "fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'puppet-physics-impulse'))

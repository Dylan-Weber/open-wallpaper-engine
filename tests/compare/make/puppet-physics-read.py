# puppet-physics-read: the middle bone of a strip of three bones turns on a spring; a script kicks it once (frame 390) and shows
# where the tip bone is, as getBoneTransform('tip') gives it (it includes the physics): a red square at 960 + 8 x the tip's
# movement along x, 700 - 8 x its movement along y... so every picture carries the numbers, exact to a fraction of a unit
# (a series of pictures one after another, compare.json "shots" and "fps": 30, each compared at its own frame).
# Variants: the middle bone's inertia (ri 100: none) and friction (rf).
def physics_read(name, title, inertia, friction, kick=3, stiffness=200):
    w = Wallpaper(name, title, clear='0 0 0')
    vertices, indices = strip_mesh(bones=3)
    one = (1, 1, 1)
    rest = [[((0, 0, 0), (0, 0, 0), one)] * 2 for _ in range(3)]
    physics = {"a": "1 0 0", "gd": "0 -1 0", "ge": False, "ik": False, "ikce": False, "ikd": 2, "ikg": True, "ikr": True,
               "ikrd": 25, "ikrmax": 3.1415927, "ikrmin": 0, "la": False, "lamax": "3.14159 3.14159 3.14159",
               "lamin": "-3.14159 -3.14159 -3.14159", "layout": "advanced", "lt": False, "ltmax": 100, "m": 20,
               "preset": "none", "presetresponsiveness": 100, "r": True, "rax": True, "ray": True, "raz": True,
               "re": False, "rf": friction, "ri": inertia, "rs": stiffness, "s": 0, "se": True, "t": False, "tax": True,
               "tay": True, "taz": True, "tf": 20, "ti": 30, "tm": 60.0, "tp": "62.5 0 0", "ts": 200}
    write_binary('%s/models/strip_puppet.mdl' % name, puppet_mdl(
        'materials/strip.json', vertices, indices,
        [('root', None, (0, 0, 0)), ('mid', 0, (150, 0, 0), json.dumps(physics)), ('tip', 1, (150, 0, 0))],
        [('idle', 30.0, rest)]))
    model = w.texture_model('strip', 'gradient/gradient_rainbow', model_extra={"puppet": "models/strip_puppet.mdl"})
    script = """'use strict';
let kicked = false;
let home = null;

function tipPlace() {
    // Wallpaper Engine gives a Mat4 (translation() is a Vec3) for a bone's name; owe, a vector for a number from 1
    const m = thisLayer.getBoneTransform('tip');
    const t = (m && typeof m.translation === 'function') ? m.translation() : thisLayer.getBoneTransform(3);
    return { x: t.x, y: t.y };
}

export function update(value) {
    if (!kicked && engine.runtime >= 13) {
        kicked = true;
        thisLayer.applyBonePhysicsImpulse('mid', new Vec3(0, 0, 0), new Vec3(0, 0, %s));
    }
    const here = tipPlace();
    if (home === null) home = here;
    thisScene.getLayer('tip x').origin = new Vec3(960 + 8 * (here.x - home.x), 900, 0);
    thisScene.getLayer('tip y').origin = new Vec3(1500, 700 + 8 * (here.y - home.y), 0);
    return value;
}
""" % kick
    w.add({"name": "strip", "image": model, "origin": {"value": "960 540 0", "script": script, "scriptproperties": {}},
           "size": "600 100", "color": "1 1 1", "alpha": 1, "angles": "0 0 0", "scale": "1 1 1", "alignment": "center",
           "animationlayers": [{"animation": 1, "blend": 1.0, "rate": 1.0, "visible": True, "id": 1, "name": "idle"}]})
    w.solid('tip x', 960, 900, 24, 24, color='1 0 0')
    w.solid('tip y', 1500, 700, 24, 24, color='0 1 0')
    w.save()
    with open(os.path.join(HERE, name, 'compare.json'), 'w') as f:
        json.dump({"shots": 140, "shot_interval_ms": 0, "fps": 30}, f)
    clock_probe.stamp(os.path.join(HERE, name))


physics_read('puppet-physics-read-ri30', 'the tip bone read while its turning spring swings (inertia 30)', 30, 3)
physics_read('puppet-physics-read-ri100', 'the tip bone read while its turning spring swings (no inertia)', 100, 3)
physics_read('puppet-physics-read-ri0', 'the tip bone read while its turning spring swings (inertia 0)', 0, 3)
physics_read('puppet-physics-read-ri60', 'the tip bone read while its turning spring swings (inertia 60)', 60, 3)

# script-throws: property scripts whose update throws: a visible script and an origin script, throwing from the first update,
# throwing after one good update, and an init that throws. OoT's interface root's visible script throws in update (it reads
# shared.camera, which another script sets): Wallpaper Engine keeps the layer shown, owe hid it.
w = Wallpaper('script-throws', 'property scripts that throw', clear='0.05 0.05 0.1')
throw_update = """'use strict';
export function update(value) {
    const broken = shared.missing.position;
    return value;
}
"""
throw_later = """'use strict';
let frames = 0;
export function update(value) {
    if (++frames > 1) shared.missing.position = 1;
    return %s;
}
"""
throw_init = """'use strict';
export function init(value) {
    shared.missing.position = 1;
    return value;
}
export function update(value) { return value; }
"""
cases = (('visible, throws in update', 250, 'visible', throw_update, True),
         ('visible, throws after a good update', 650, 'visible', throw_later % 'value', True),
         ('visible, throws in init', 1050, 'visible', throw_init, True),
         ('origin, throws in update', 1450, 'origin', throw_update, None),
         ('origin, moved then throws', 1750, 'origin', throw_later % 'new Vec3(value.x, value.y - 300, 0)', None))
for name, x, prop, script, _ in cases:
    layer = {"name": name, "image": "models/util/solidlayer.json", "size": "200 200", "color": "1 0.6 0.2", "alpha": 1,
             "origin": "%d 700 0" % x, "angles": "0 0 0", "scale": "1 1 1", "alignment": "center", "visible": True}
    layer[prop] = {"value": layer[prop], "script": script, "scriptproperties": {}}
    w.add(layer)
w.save()
with open(os.path.join(HERE, 'script-throws', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'script-throws'))

# puppet-bone-init: in a script's init, Wallpaper Engine's bones read zero and setting them does nothing. The idle animation turns
# the middle bone 0.3 radians. The script's init reads that angle (square "init read", 960 + 200 * what it read) and tries
# to turn the tip 1.5 (nothing happens: the strip bends only at its middle); update reads it again (square "update read").
w = Wallpaper('puppet-bone-init', 'bones read and set from a script\'s init', clear='0 0 0')
vertices, indices = strip_mesh(bones=3)
one = (1, 1, 1)
rest = [[((0, 0, 0), (0, 0, 0), one)] * 2,
        [((0, 0, 0), (0, 0, 0.3), one)] * 2,
        [((0, 0, 0), (0, 0, 0), one)] * 2]
write_binary('puppet-bone-init/models/strip_puppet.mdl', puppet_mdl(
    'materials/strip.json', vertices, indices, [('root', None, (0, 0, 0)), ('mid', 0, (150, 0, 0)), ('tip', 1, (150, 0, 0))],
    [('idle', 30.0, rest)]))
model = w.texture_model('strip', 'gradient/gradient_rainbow', model_extra={"puppet": "models/strip_puppet.mdl"})
script = """'use strict';
let atInit = 0;

export function init(value) {
    atInit = thisLayer.getLocalBoneAngles('mid').z;
    thisLayer.setLocalBoneAngles('tip', new Vec3(0, 0, 1.5));
    return value;
}

export function update(value) {
    thisScene.getLayer('init read').origin = new Vec3(960 + 200 * atInit, 800, 0);
    thisScene.getLayer('update read').origin = new Vec3(960 + 200 * thisLayer.getLocalBoneAngles('mid').z, 900, 0);
    return value;
}
"""
w.add({"name": "strip", "image": model, "origin": {"value": "960 400 0", "script": script, "scriptproperties": {}},
       "size": "600 100", "color": "1 1 1", "alpha": 1, "angles": "0 0 0", "scale": "1 1 1", "alignment": "center",
       "animationlayers": [{"animation": 1, "blend": 1.0, "rate": 1.0, "visible": True, "id": 1, "name": "idle"}]})
w.solid('init read', 960, 800, 40, 40, color='1 0 0')
w.solid('update read', 960, 900, 40, 40, color='0 1 0')
w.save()

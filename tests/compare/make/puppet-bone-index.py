# puppet-bone-index: what a script is told of a puppet's bones by name. Wallpaper Engine numbers them from 0 and says
# -1 for a name the layer doesn't have, the empty name included (owe #5). Each white square sits at 300 + 150 * (the
# number + 1): bone 'root' first, then 'mid', 'tip', and a name that isn't there.
w = Wallpaper('puppet-bone-index', 'bone numbers a script is given', clear='0 0 0')
vertices, indices = strip_mesh(bones=3)
write_binary('puppet-bone-index/models/strip_puppet.mdl', puppet_mdl(
    'materials/strip.json', vertices, indices, [('root', None, (0, 0, 0)), ('mid', 0, (0, 0, 0)), ('tip', 1, (0, 0, 0))]))
model = w.texture_model('strip', 'gradient/gradient_rainbow', model_extra={"puppet": "models/strip_puppet.mdl"})
script = """'use strict';
export function init(value) {
    const names = ['root', 'mid', 'tip', 'missing', ''];
    for (let i = 0; i < names.length; i++) {
        thisScene.getLayer('mark ' + i).origin = new Vec3(300 + 150 * (thisLayer.getBoneIndex(names[i]) + 1), 800 - i * 100, 0);
    }
    return value;
}
export function update(value) { return value; }
"""
w.add({"name": "strip", "image": model, "origin": {"value": "960 200 0", "script": script, "scriptproperties": {}},
       "size": "600 100", "color": "1 1 1", "alpha": 1, "angles": "0 0 0", "scale": "1 1 1", "alignment": "center"})
for i in range(5):
    w.solid('mark %d' % i, 100, 800 - i * 100, 40, 40)
w.save()

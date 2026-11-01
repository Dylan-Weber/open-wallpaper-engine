# puppet-bone-probe: what getBoneTransform gives (diagnostic): 16 squares, each shown when a fact about the value holds, for
# the bone's name 'tip' (top row) and its number 2 (bottom row): an object, has a number x, has translation, translation has x,
# an array of 16, has getTranslation, translation is a function, has m (columns)
w = Wallpaper('puppet-bone-probe', 'what getBoneTransform returns', clear='0 0 0')
vertices, indices = strip_mesh(bones=3)
one = (1, 1, 1)
rest = [[((0, 0, 0), (0, 0, 0), one)] * 2 for _ in range(3)]
write_binary('puppet-bone-probe/models/strip_puppet.mdl', puppet_mdl(
    'materials/strip.json', vertices, indices, [('root', None, (0, 0, 0)), ('mid', 0, (150, 0, 0)), ('tip', 1, (150, 0, 0))],
    [('idle', 30.0, rest)]))
model = w.texture_model('strip', 'gradient/gradient_rainbow', model_extra={"puppet": "models/strip_puppet.mdl"})
script = """'use strict';
export function update(value) {
    const facts = (m) => [
        typeof m === 'object' && m !== null,
        m && typeof m.x === 'number',
        m && !!m.translation,
        m && m.translation && typeof m.translation.x === 'number',
        m && typeof m.length === 'number' && m.length === 16,
        m && typeof m.getTranslation === 'function',
        m && typeof m.translation === 'function',
        m && typeof m.m === 'object',
    ];
    let a = [], b = [];
    try { a = facts(thisLayer.getBoneTransform('tip')); } catch (e) { }
    try { b = facts(thisLayer.getBoneTransform(2)); } catch (e) { }
    for (let i = 0; i < 8; i++) {
        thisScene.getLayer('a' + i).visible = !!a[i];
        thisScene.getLayer('b' + i).visible = !!b[i];
    }
    return value;
}
"""
w.add({"name": "strip", "image": model, "origin": {"value": "960 540 0", "script": script, "scriptproperties": {}},
       "size": "600 100", "color": "1 1 1", "alpha": 1, "angles": "0 0 0", "scale": "1 1 1", "alignment": "center",
       "animationlayers": [{"animation": 1, "blend": 1.0, "rate": 1.0, "visible": True, "id": 1, "name": "idle"}]})
for i in range(8):
    w.solid('a%d' % i, 200 + 200 * i, 900, 60, 60, color='1 0 0')
    w.solid('b%d' % i, 200 + 200 * i, 200, 60, 60, color='0 1 0')
w.save()

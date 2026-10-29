# puppet-bone-parents: how many bones a puppet has and each bone's parent, as a script is told: numbered from 0, -1 for
# a root, a missing name or a number past the last bone (owe #6). Square i sits at 300 + 150 * (its answer + 1):
# the count, then the parents of 'tip', 'mid', 'root', 'missing' and of bone 9.
w = Wallpaper('puppet-bone-parents', 'bone count and parents a script is given', clear='0 0 0')
vertices, indices = strip_mesh(bones=3)
write_binary('puppet-bone-parents/models/strip_puppet.mdl', puppet_mdl(
    'materials/strip.json', vertices, indices, [('root', None, (0, 0, 0)), ('mid', 0, (0, 0, 0)), ('tip', 1, (0, 0, 0))]))
model = w.texture_model('strip', 'gradient/gradient_rainbow', model_extra={"puppet": "models/strip_puppet.mdl"})
script = """'use strict';
export function init(value) {
    const answers = [thisLayer.getBoneCount(), thisLayer.getBoneParentIndex('tip'), thisLayer.getBoneParentIndex('mid'),
                     thisLayer.getBoneParentIndex('root'), thisLayer.getBoneParentIndex('missing'),
                     thisLayer.getBoneParentIndex(9)];
    for (let i = 0; i < answers.length; i++) {
        thisScene.getLayer('mark ' + i).origin = new Vec3(300 + 150 * (answers[i] + 1), 900 - i * 100, 0);
    }
    return value;
}
export function update(value) { return value; }
"""
w.add({"name": "strip", "image": model, "origin": {"value": "960 200 0", "script": script, "scriptproperties": {}},
       "size": "600 100", "color": "1 1 1", "alpha": 1, "angles": "0 0 0", "scale": "1 1 1", "alignment": "center"})
for i in range(6):
    w.solid('mark %d' % i, 100, 900 - i * 100, 40, 40)
w.save()

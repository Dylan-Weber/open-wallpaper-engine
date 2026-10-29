# puppet-local-bone: a script poses bones relative to their parent every frame: the middle bone turns 0.5 and the tip 4
# (radians), so the strip bends twice (owe #35). The white square sits at 960 + 200 * what the tip's angle reads back,
# which Wallpaper Engine keeps between -pi and pi.
w = Wallpaper('puppet-local-bone', 'local bone angles set by a script', clear='0 0 0')
vertices, indices = strip_mesh(bones=3)
one = (1, 1, 1)
rest = [[((0, 0, 0), (0, 0, 0), one)] * 2 for _ in range(3)]
write_binary('puppet-local-bone/models/strip_puppet.mdl', puppet_mdl(
    'materials/strip.json', vertices, indices, [('root', None, (0, 0, 0)), ('mid', 0, (150, 0, 0)), ('tip', 1, (150, 0, 0))],
    [('idle', 30.0, rest)]))
model = w.texture_model('strip', 'gradient/gradient_rainbow', model_extra={"puppet": "models/strip_puppet.mdl"})
script = """'use strict';
export function update(value) {
    thisLayer.setLocalBoneAngles('mid', new Vec3(0, 0, 0.5));
    thisLayer.setLocalBoneAngles('tip', new Vec3(0, 0, 4));
    const angles = thisLayer.getLocalBoneAngles('tip');
    thisScene.getLayer('mark').origin = new Vec3(960 + 200 * angles.z, 800, 0);
    return value;
}
"""
w.add({"name": "strip", "image": model, "origin": {"value": "960 400 0", "script": script, "scriptproperties": {}},
       "size": "600 100", "color": "1 1 1", "alpha": 1, "angles": "0 0 0", "scale": "1 1 1", "alignment": "center",
       "animationlayers": [{"animation": 1, "blend": 1.0, "rate": 1.0, "visible": True, "id": 1, "name": "idle"}]})
w.solid('mark', 960, 800, 40, 40)
w.save()

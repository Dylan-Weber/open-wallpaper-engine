# puppet-layer-playing: whether a puppet's animation layers are playing from the start, as a script sees it
# (getAnimationLayer(name).isPlaying(), and Ocarina of Time's getAnimationLayer(name).getAnimation(name).isPlaying()).
# Three looping layers: blend 1, blend 0, and a blend script that returns 0. Each marker sits high when its layer says
# it is playing, low when not, and at the bottom when the script throws. Ocarina of Time turns Link only while his
# "Surprise" layer (blend from a script) plays: Wallpaper Engine's Link had turned, owe's hadn't.
w = Wallpaper('puppet-layer-playing', "whether a puppet's animation layers play from the start", clear='0 0 0')
vertices, indices = strip_mesh(bones=3)
one = (1, 1, 1)
still = [((0, 0, 0), (0, 0, 0), one)] * 4
animations = [('idle', 30.0, [still, still, still]), ('zero', 30.0, [still, still, still]),
              ('scripted', 30.0, [still, still, still])]
write_binary('puppet-layer-playing/models/strip_puppet.mdl', puppet_mdl(
    'materials/strip.json', vertices, indices,
    [('root', None, (0, 0, 0)), ('mid', 0, (150, 0, 0)), ('tip', 1, (150, 0, 0))], animations))
model = w.texture_model('strip', 'gradient/gradient_rainbow', model_extra={"puppet": "models/strip_puppet.mdl"})
layers = [("Idle", 1, 1.0), ("Zero", 2, 0.0),
          ("Scripted", 3, {"value": 1.0, "scriptproperties": {}, "script": "'use strict';\nexport function update(value) {\n    return 0;\n}\n"})]
w.add({"name": "strip", "image": model, "origin": "960 950 0", "size": "600 100", "color": "1 1 1", "alpha": 1,
       "angles": "0 0 0", "scale": "1 1 1", "alignment": "center",
       "animationlayers": [{"animation": a, "additive": False, "blend": blend, "blendin": False, "blendout": False,
                            "blendtime": 0.5, "rate": 1.0, "visible": True, "id": 100 + k, "name": name}
                           for k, (name, a, blend) in enumerate(layers)]})
marker = """'use strict';
export function update(value) {
    try {
        const playing = %s;
        return new Vec3(value.x, playing ? 700 : 450, 0);
    } catch (e) {
        return new Vec3(value.x, 200, 0);
    }
}
"""
for k, (name, _, _) in enumerate(layers):
    for j, call in enumerate(("thisScene.getLayer('strip').getAnimationLayer('%s').isPlaying()" % name,
                              "thisScene.getLayer('strip').getAnimationLayer('%s').getAnimation('x').isPlaying()"
                              % name)):
        x = 360 + 240 * (2 * k + j)
        w.add({"name": "%s %d" % (name, j), "image": "models/util/solidlayer.json", "size": "120 120",
               "color": ("1 0.3 0.3", "0.3 1 0.3", "0.4 0.5 1")[k], "alpha": 1, "angles": "0 0 0", "scale": "1 1 1",
               "alignment": "center", "visible": True,
               "origin": {"value": "%d 450 0" % x, "scriptproperties": {}, "script": marker % call}})
w.save()
with open(os.path.join(HERE, 'puppet-layer-playing', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'puppet-layer-playing'))


# puppet-layer-getanimation: what an animation layer's getAnimation is in Wallpaper Engine (puppet-layer-playing showed
# getAnimationLayer(name).getAnimation('x').isPlaying() throws there): markers high when the expression is true, low
# when false, at the bottom when it throws.
w = Wallpaper('puppet-layer-getanimation', "an animation layer's getAnimation", clear='0 0 0')
write_binary('puppet-layer-getanimation/models/strip_puppet.mdl', puppet_mdl(
    'materials/strip.json', vertices, indices,
    [('root', None, (0, 0, 0)), ('mid', 0, (150, 0, 0)), ('tip', 1, (150, 0, 0))], animations))
model = w.texture_model('strip', 'gradient/gradient_rainbow', model_extra={"puppet": "models/strip_puppet.mdl"})
w.add({"name": "strip", "image": model, "origin": "960 950 0", "size": "600 100", "color": "1 1 1", "alpha": 1,
       "angles": "0 0 0", "scale": "1 1 1", "alignment": "center",
       "animationlayers": [{"animation": 1, "additive": False, "blend": 1.0, "blendin": False, "blendout": False,
                            "blendtime": 0.5, "rate": 1.0, "visible": True, "id": 100, "name": "Idle"}]})
layer = "thisScene.getLayer('strip').getAnimationLayer('Idle')"
for k, test in enumerate(("typeof %s.getAnimation === 'function'" % layer,
                          "%s.getAnimation() !== undefined" % layer,
                          "%s.getAnimation().isPlaying()" % layer,
                          "%s.getAnimation('Idle').isPlaying()" % layer,
                          "%s.getAnimation('idle').isPlaying()" % layer,
                          "thisScene.getLayer('strip').getAnimation('idle') !== undefined")):
    w.add({"name": "test %d" % k, "image": "models/util/solidlayer.json", "size": "120 120",
           "color": "0.4 0.5 1" if k % 2 else "1 0.6 0.2", "alpha": 1, "angles": "0 0 0", "scale": "1 1 1",
           "alignment": "center", "visible": True,
           "origin": {"value": "%d 450 0" % (360 + 240 * k), "scriptproperties": {}, "script": marker % test}})
w.save()
with open(os.path.join(HERE, 'puppet-layer-getanimation', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'puppet-layer-getanimation'))

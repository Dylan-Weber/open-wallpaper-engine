# puppet-layer-this: what `thisObject` is in an animation layer's own script (its `visible` here), and whether
# thisObject.getAnimation() works there. getAnimationLayer(name).getAnimation() is undefined in Wallpaper Engine
# (puppet-layer-getanimation), yet a Workshop wallpaper (3462491575) starts its layers' playback with
# `thisObject.getAnimation().play()` in a visible script. Each marker sits high when its test is true, low when
# false and at the bottom when it throws; the script's init moves the markers (a marker script can't see the layer).
w = Wallpaper('puppet-layer-this', "thisObject in an animation layer's script", clear='0 0 0')
vertices, indices = strip_mesh(bones=3)
one = (1, 1, 1)
still = [((0, 0, 0), (0, 0, 0), one)] * 4
animations = [('idle', 30.0, [still, still, still])]
write_binary('puppet-layer-this/models/strip_puppet.mdl', puppet_mdl(
    'materials/strip.json', vertices, indices,
    [('root', None, (0, 0, 0)), ('mid', 0, (150, 0, 0)), ('tip', 1, (150, 0, 0))], animations))
model = w.texture_model('strip', 'gradient/gradient_rainbow', model_extra={"puppet": "models/strip_puppet.mdl"})
tests = ("typeof thisObject.getAnimation === 'function'",
         "thisObject.getAnimation() !== undefined",
         "thisObject.getAnimation().isPlaying()",
         "typeof thisObject.getFrame === 'function'",
         "typeof thisObject.play === 'function'",
         "'addEndedCallback' in thisObject",
         "thisObject.frameCount > 0")
script = "'use strict';\nexport function init(value) {\n"
for k, test in enumerate(tests):
    script += """    try {
        thisScene.getLayer('marker %d').origin = new Vec3(%d, (%s) ? 700 : 450, 0);
    } catch (e) {
        thisScene.getLayer('marker %d').origin = new Vec3(%d, 200, 0);
    }
""" % (k, 360 + 240 * k, test, k, 360 + 240 * k)
script += "    return value;\n}\nexport function update(value) { return value; }\n"
w.add({"name": "strip", "image": model, "origin": "960 950 0", "size": "600 100", "color": "1 1 1", "alpha": 1,
       "angles": "0 0 0", "scale": "1 1 1", "alignment": "center",
       "animationlayers": [{"animation": 1, "additive": False, "blend": 1.0, "blendin": False, "blendout": False,
                            "blendtime": 0.5, "rate": 1.0, "id": 100, "name": "Idle",
                            "visible": {"value": True, "scriptproperties": {}, "script": script}}]})
for k in range(len(tests)):
    w.add({"name": "marker %d" % k, "image": "models/util/solidlayer.json", "size": "120 120",
           "color": "0.4 0.5 1" if k % 2 else "1 0.6 0.2", "alpha": 1, "angles": "0 0 0", "scale": "1 1 1",
           "alignment": "center", "visible": True, "origin": "%d 450 0" % (360 + 240 * k)})
w.save()
with open(os.path.join(HERE, 'puppet-layer-this', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'puppet-layer-this'))

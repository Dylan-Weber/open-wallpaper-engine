# script-create-unregistered: thisScene.createLayer with an asset the script never registered (engine.registerAsset). Reze's
# audio visualizer creates its 63 bars from 'models/bar.json' without registering it; Wallpaper Engine draws none of them, owe
# all. Squares made from: the shipped solid layer registered, the shipped solid layer unregistered (both by path), the
# wallpaper's own model registered and unregistered, and the shipped solid layer unregistered as an object.
w = Wallpaper('script-create-unregistered', 'createLayer with and without registerAsset', clear='0.05 0.05 0.1')
w.files['models/square.json'] = {"material": "materials/square.json"}
w.files['materials/square.json'] = {"passes": [{"blending": "normal", "cullmode": "nocull", "depthtest": "disabled",
                                                "depthwrite": "disabled", "shader": "flat", "textures": ["util/white"]}]}
cases = (('shipped, registered', 300, 'models/util/solidlayer.json', True, 'path'),
         ('shipped, unregistered', 650, 'models/util/solidlayer.json', False, 'path'),
         ('own, registered', 1000, 'models/square.json', True, 'path'),
         ('own, unregistered', 1350, 'models/square.json', False, 'path'),
         ('shipped, unregistered, object', 1700, 'models/util/solidlayer.json', False, 'object'))
for name, x, asset, registered, form in cases:
    make = ("thisScene.createLayer('%s')" % asset if form == 'path' else
            "thisScene.createLayer({image: '%s', size: new Vec2(200, 200), color: new Vec3(1, 1, 1)})" % asset)
    script = """'use strict';
%sexport function init(value) {
    let made = %s;
    made.origin = new Vec3(%d, 540, 0);
    made.scale = new Vec3(%s);
    return value;
}
export function update(value) { return value; }
""" % ("engine.registerAsset('%s');\n" % asset if registered else '', make, x, '1, 1, 1' if form == 'object' else '4, 4, 1')
    w.add({"name": name, "image": "models/util/solidlayer.json", "size": "10 10", "color": "0 0 0", "alpha": 0,
           "origin": {"value": "%d 900 0" % x, "script": script, "scriptproperties": {}}, "angles": "0 0 0",
           "scale": "1 1 1", "alignment": "center"})
w.save()
with open(os.path.join(HERE, 'script-create-unregistered', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'script-create-unregistered'))

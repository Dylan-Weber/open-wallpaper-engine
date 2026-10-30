# texture-animation-created: the rose petal sprite sheet on a layer there from the start, on one a script creates in
# init, and on one it creates after two seconds: which frame each shows at the probed moment. Dino Run's coins are made
# with createLayer every five seconds and show another frame in owe than in Wallpaper Engine (does a created layer's
# animation start when it is created?).
w = Wallpaper('texture-animation-created', 'sprite sheet animations of created layers', clear='0.1 0.1 0.2')
petals = w.texture_model('petals', 'particle/nature/rosepetals', model_extra={"autosize": True})
w.add({"name": "from the start", "image": petals, "size": "256 256", "color": "1 1 1", "alpha": 1,
       "angles": "0 0 0", "scale": "1 1 1", "alignment": "center", "visible": True, "origin": "400 600 0"})
script = """'use strict';
const asset = engine.registerAsset('%s');
let made = false;
export function init(value) {
    const layer = thisScene.createLayer(asset);
    layer.origin = new Vec3(960, 600, 0);
    return value;
}
export function update(value) {
    if (!made && engine.runtime >= 2) {
        made = true;
        const layer = thisScene.createLayer(asset);
        layer.origin = new Vec3(1520, 600, 0);
    }
    return value;
}
""" % petals
w.add({"name": "maker", "image": "models/util/solidlayer.json", "size": "10 10", "color": "0 0 0", "alpha": 0,
       "angles": "0 0 0", "scale": "1 1 1", "alignment": "center", "visible": True,
       "origin": {"value": "10 10 0", "script": script, "scriptproperties": {}}})
w.save()
with open(os.path.join(HERE, 'texture-animation-created', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'texture-animation-created'))

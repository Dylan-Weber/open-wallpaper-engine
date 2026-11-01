# texture-animation-created-phase: does a created layer's sprite animation start when it is created, or run on the
# scene's clock? The rose petal sheet (5 frames of 0.2 s, 1 s round) on a layer there from the start, and on layers a
# script creates at 2.1, 2.3 and 2.5 s, offsets that land on different frames either way (texture-animation-created
# creates at 2 s, a whole round, which can't tell). Dino Run's coins show another frame in owe.
w = Wallpaper('texture-animation-created-phase', 'sprite phase of layers created mid-round', clear='0.1 0.1 0.2')
petals = w.texture_model('petals', 'particle/nature/rosepetals', model_extra={"autosize": True})
w.add({"name": "from the start", "image": petals, "size": "256 256", "color": "1 1 1", "alpha": 1,
       "angles": "0 0 0", "scale": "1 1 1", "alignment": "center", "visible": True, "origin": "250 600 0"})
script = """'use strict';
const asset = engine.registerAsset('%s');
const times = [2.1, 2.3, 2.5];
let made = 0;
export function update(value) {
    while (made < times.length && engine.runtime >= times[made]) {
        const layer = thisScene.createLayer(asset);
        layer.origin = new Vec3(650 + 400 * made, 600, 0);
        made++;
    }
    return value;
}
""" % petals
w.add({"name": "maker", "image": "models/util/solidlayer.json", "size": "10 10", "color": "0 0 0", "alpha": 0,
       "angles": "0 0 0", "scale": "1 1 1", "alignment": "center", "visible": True,
       "origin": {"value": "10 10 0", "script": script, "scriptproperties": {}}})
w.save()
with open(os.path.join(HERE, 'texture-animation-created-phase', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'texture-animation-created-phase'))

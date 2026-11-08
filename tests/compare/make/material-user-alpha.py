# material-user-alpha: a material whose Alpha constant is bound to a user property (opacity 0, and half 0.5), on a layer of the
# scene and on a layer a script creates from the same model. Reze's audio visualizer bars have their Alpha bound to its
# "opacity" (0): Wallpaper Engine draws them transparent, owe drew the created ones opaque.
w = Wallpaper('material-user-alpha', "a material's Alpha bound to a user property", clear='0.05 0.05 0.1')
w.properties['opacity'] = {"index": 0, "order": 100, "text": "Opacity", "type": "slider", "min": 0, "max": 1, "value": 0}
w.properties['half'] = {"index": 1, "order": 101, "text": "Half", "type": "slider", "min": 0, "max": 1, "value": 0.5}
for prop in ('opacity', 'half'):
    w.files['models/%s.json' % prop] = {"material": "materials/%s.json" % prop}
    w.files['materials/%s.json' % prop] = {"passes": [{
        "blending": "translucent", "cullmode": "nocull", "depthtest": "disabled", "depthwrite": "disabled",
        "shader": "genericimage2", "textures": ["gradient/gradient_rainbow"],
        "constantshadervalues": {"Alpha": {"user": prop, "value": 1}}}]}
created = """'use strict';
engine.registerAsset('models/opacity.json');
engine.registerAsset('models/half.json');
export function init(value) {
    for (const [model, x] of [['models/opacity.json', 1150], ['models/half.json', 1600]]) {
        let made = thisScene.createLayer(model);
        made.origin = new Vec3(x, 540, 0);
        made.scale = new Vec3(5, 150, 1);
    }
    return value;
}
export function update(value) { return value; }
"""
for prop, x in (('opacity', 300), ('half', 700)):
    w.add({"name": "scene " + prop, "image": "models/%s.json" % prop, "origin": "%d 540 0" % x, "size": "320 300",
           "color": "1 1 1", "alpha": 1, "angles": "0 0 0", "scale": "1 1 1", "alignment": "center"})
w.add({"name": "creator", "image": "models/util/solidlayer.json", "size": "10 10", "color": "0 0 0", "alpha": 0,
       "origin": {"value": "10 10 0", "script": created, "scriptproperties": {}}, "angles": "0 0 0", "scale": "1 1 1",
       "alignment": "center"})
w.save()
with open(os.path.join(HERE, 'material-user-alpha', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'material-user-alpha'))

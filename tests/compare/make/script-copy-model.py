# script-copy-model: a script makes three layers from a model layer's initial configuration (getInitialLayerConfig, as
# More Physics spawns its spheres) and places them; they keep the model and its material. Wallpaper Engine shows
# four red cubes; owe made the copies plain squares.
w = Scene3D('script-copy-model', 'a script copies a model layer', ambient='1 1 1')
red = w.material('red', '1 0 0')
w.cube('source', red, -3, 0, 0, (0.3, 0.5, 0), '1.2 1.2 1.2')
script = """'use strict';
export function init(value) {
    const source = thisScene.getLayer('source');
    for (let i = 1; i <= 3; i++) {
        const config = thisScene.getInitialLayerConfig(source);
        config.name = 'copy ' + i;
        const copy = thisScene.createLayer(config);
        copy.origin = new Vec3(-3 + i * 2.4, (i % 2) * 1.5, -i);
        copy.scale = new Vec3(1.2 - i * 0.2, 1.2 - i * 0.2, 1.2 - i * 0.2);
    }
    return value;
}
export function update(value) { return value; }
"""
w.add({"name": "driver", "image": "models/util/solidlayer.json", "size": "10 10", "color": "0 0 0", "alpha": 0,
       "origin": {"value": "10 10 0", "script": script, "scriptproperties": {}}, "angles": "0 0 0",
       "scale": "1 1 1", "alignment": "center"})
w.save()
with open(os.path.join(HERE, 'script-copy-model', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'script-copy-model'))

# script-layer-alignment: a script sets a layer's alignment, then its scale, inside try/catch (as Reze's audio visualizer,
# whose 63 bars Wallpaper Engine leaves at their tiny default size and owe scales). Squares: scaled only; alignment set to
# its own value first; alignment set to another value first; alignment read and compared first (as Reze); and, for a
# layer the script created, alignment then scale.
w = Wallpaper('script-layer-alignment', "setting a layer's alignment from a script", clear='0.05 0.05 0.1')
cases = (('scale only', 300, ''), ('same alignment first', 650, "layer.alignment = 'center';\n"),
         ('other alignment first', 1000, "layer.alignment = 'bottom';\n"),
         ('compared, then set', 1350, "if (layer.alignment !== 'top') { layer.alignment = 'top'; }\n"))
for name, x, before in cases:
    script = """'use strict';
export function init(value) {
    const layer = thisLayer;
    try {
        %s        layer.scale = new Vec3(20, 20, 1);
    } catch (error) { console.error(error); }
    return value;
}
export function update(value) { return value; }
""" % before.replace('\n', '\n        ') if before else """'use strict';
export function init(value) {
    try { thisLayer.scale = new Vec3(20, 20, 1); } catch (error) { console.error(error); }
    return value;
}
export function update(value) { return value; }
"""
    w.add({"name": name, "image": "models/util/solidlayer.json", "size": "10 10", "color": "1 0.6 0.2", "alpha": 1,
           "origin": {"value": "%d 540 0" % x, "script": script, "scriptproperties": {}}, "angles": "0 0 0",
           "alignment": "center", "scale": "1 1 1"})
created = """'use strict';
engine.registerAsset('models/util/solidlayer.json', true);
export function init(value) {
    let made = thisScene.createLayer('models/util/solidlayer.json');
    made.origin = new Vec3(1700, 540, 0);
    try {
        made.alignment = 'center';
        made.scale = new Vec3(20, 20, 1);
    } catch (error) { console.error(error); }
    return value;
}
export function update(value) { return value; }
"""
w.add({"name": "creator", "image": "models/util/solidlayer.json", "size": "10 10", "color": "0.2 0.6 1", "alpha": 1,
       "origin": {"value": "1700 900 0", "script": created, "scriptproperties": {}}, "angles": "0 0 0",
       "scale": "1 1 1", "alignment": "center"})
w.save()
with open(os.path.join(HERE, 'script-layer-alignment', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'script-layer-alignment'))

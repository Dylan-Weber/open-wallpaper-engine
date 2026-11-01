# script-hidden-layer: do a hidden layer's scripts run? Each driver layer's script creates a square in init: a visible driver,
# a hidden one, one hidden by a user property that is off, and one whose own visible property carries the script (as Reze's
# audio visualizer, whose hidden layer's init creates 63 bars that Wallpaper Engine doesn't draw and owe does).
w = Wallpaper('script-hidden-layer', "a hidden layer's scripts", clear='0.05 0.05 0.1')
w.properties['show'] = {"index": 0, "order": 100, "text": "Show", "type": "bool", "value": False}
def creator(x, colour):
    return """'use strict';
engine.registerAsset('models/util/solidlayer.json', true);
export function init(value) {
    thisScene.createLayer({image: 'models/util/solidlayer.json', name: 'made %d', origin: new Vec3(%d, 540, 0),
                           size: new Vec2(200, 200), color: new Vec3(%s)});
    return value;
}
export function update(value) { return value; }
""" % (x, x, ', '.join(colour.split()))
drivers = (('visible driver', 160, '1 0.3 0.3', True, 'origin'), ('hidden driver', 560, '0.3 1 0.3', False, 'origin'),
           ('hidden by a user property', 960, '0.3 0.3 1', {"user": "show", "value": False}, 'origin'),
           ('script on its visible', 1360, '1 1 0.3', False, 'visible'),
           ('script on its visible, bound to a user property', 1760, '1 0.3 1', False, 'visible user'))
for name, x, colour, visible, scripted in drivers:
    layer = {"name": name, "image": "models/util/solidlayer.json", "size": "20 20", "color": "1 1 1", "alpha": 1,
             "origin": "%d 900 0" % x, "angles": "0 0 0", "scale": "1 1 1", "alignment": "center", "visible": visible}
    key = scripted.split()[0]
    layer[key] = {"value": layer[key] if key == 'origin' else False, "script": creator(x, colour), "scriptproperties": {}}
    if scripted == 'visible user':
        layer[key]["user"] = "show"
    w.add(layer)
w.save()
with open(os.path.join(HERE, 'script-hidden-layer', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'script-hidden-layer'))

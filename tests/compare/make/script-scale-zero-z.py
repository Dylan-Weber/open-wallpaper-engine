# script-scale-zero-z: 2D layers whose scale has a zero z, set in the scene and by a script (Reze's audio visualizer scales its
# bars by (width, height, 0); Wallpaper Engine draws none of them, owe all): z 1, z 0 from the scene, z 0 from a script, x 0.
w = Wallpaper('script-scale-zero-z', 'a 2D layer scaled to zero along z', clear='0.05 0.05 0.1')
w.solid('z 1', 300, 540, 10, 10, '1 0.6 0.2', scale='20 20 1')
w.solid('z 0', 700, 540, 10, 10, '0.2 1 0.6', scale='20 20 0')
script = """'use strict';
export function init(value) {
    thisLayer.scale = new Vec3(20, 20, 0);
    return value;
}
export function update(value) { return value; }
"""
w.add({"name": "z 0 by a script", "image": "models/util/solidlayer.json", "size": "10 10", "color": "0.6 0.2 1",
       "alpha": 1, "origin": {"value": "1100 540 0", "script": script, "scriptproperties": {}}, "angles": "0 0 0",
       "scale": "1 1 1", "alignment": "center"})
w.solid('x 0', 1500, 540, 10, 10, '1 1 0.2', scale='0 20 1')
w.save()
with open(os.path.join(HERE, 'script-scale-zero-z', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'script-scale-zero-z'))

# 31. script-lookup: what a script finds when it starts. Every layer of the scene exists by then, the ones after its
# own too; the layers that come back are counted by moving the white squares
w = Wallpaper('script-lookup', 'what scripts find when they start', clear='0 0 0')
lookup_script = """'use strict';
export function init(value) {
    let found = 0;
    for (const n of %s) { try { if (thisScene.getLayer(n)) found++; } catch (e) {} }
    let count = -1;
    try { count = thisScene.getLayerCount(); } catch (e) {}
    return new Vec3(%d + 10 * count, 200 + 100 * found, 0);
}
export function update(value) { return value; }
"""


def lookup_driver(name, x, names):
    w.add({"name": name, "image": "models/util/solidlayer.json", "size": "30 30", "color": "1 1 1", "alpha": 1,
           "origin": {"value": "%d 540 0" % x, "script": lookup_script % (names, x), "scriptproperties": {}},
           "angles": "0 0 0", "scale": "1 1 1", "alignment": "center"})


lookup_driver('first', 300, "['first','second','third','late']")
w.solid('second', 800, 540, 20, 20, '0.3 0.3 0.3')
w.solid('third', 1000, 540, 20, 20, '0.3 0.3 0.3')
lookup_driver('last', 1500, "['first','second','third','last','nonexistent']")
w.save()

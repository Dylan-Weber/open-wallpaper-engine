# pointer-script-read: what a script's cursor values are with the pointer outside the window (the capture parks it in the
# screen's corner: the shaders see 1.19, 1.16): three layers placed from input.cursorWorldPosition, input.cursorScreenPosition and
# the layer's own origin + 400 x (the cursor's world x relative to the canvas centre / the canvas width), all scaled to stay in view.
w = Wallpaper('pointer-script-read', 'script cursor values with the pointer outside the window', clear='0 0 0')


def reader(name, color, expression):
    script = """'use strict';

export function update(value) {
	return new Vec3(%s);
}
""" % expression
    w.add({"name": name, "image": "models/util/solidlayer.json", "size": "100 100", "color": color, "alpha": 1,
           "origin": {"value": "0 0 0", "script": script, "scriptproperties": {}}, "angles": "0 0 0", "scale": "1 1 1",
           "alignment": "center"})


reader('world', '1 0 0', 'input.cursorWorldPosition.x * 0.4, input.cursorWorldPosition.y * 0.4, 0')
reader('screen', '0 1 0', 'input.cursorScreenPosition.x * 0.4, input.cursorScreenPosition.y * 0.4, 0')
reader('world relative', '0 0 1', '960 + 400 * (input.cursorWorldPosition.x - 960) / 1920, 540 + 400 * (input.cursorWorldPosition.y - 540) / 1080, 0')
w.save()
with open(os.path.join(HERE, 'pointer-script-read', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'pointer-script-read'))

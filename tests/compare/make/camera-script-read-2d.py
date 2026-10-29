# camera-script-read-2d: what thisScene.getCameraTransforms() gives in a 2D scene: layers whose origin scripts put them where the
# eye's x and y, the centre's x and y, and the eye's and centre's z (scaled) say.
w = Wallpaper('camera-script-read-2d', 'a script reads the camera of a 2D scene')


def reader(name, color, expression):
    script = """'use strict';

export function update(value) {
	const t = thisScene.getCameraTransforms();
	return new Vec3(%s);
}
""" % expression
    w.add({"name": name, "image": "models/util/solidlayer.json", "size": "120 120", "color": color, "alpha": 1,
           "origin": {"value": "0 0 0", "script": script, "scriptproperties": {}}, "angles": "0 0 0", "scale": "1 1 1",
           "alignment": "center"})


reader('eye xy', '1 0 0', 't.eye.x, t.eye.y, 0')
reader('centre xy', '0 1 0', 't.center.x, t.center.y, 0')
reader('eye z', '0 0 1', '200 + t.eye.z / 4, 800, 0')
reader('centre z', '1 1 0', '200 + t.center.z / 4, 600, 0')
reader('up', '1 0 1', '1600 + t.up.x * 200, 200 + t.up.y * 200, 0')
w.save()

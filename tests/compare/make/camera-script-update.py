# camera-script-update: the same call from update(value) moves the camera, as in Wallpaper Engine: eye 6, the cubes close.
w = Scene3D('camera-script-update', 'a camera set from a script\'s update')
for i, color in enumerate(('1 0.3 0.3', '0.3 1 0.3', '0.3 0.3 1')):
    w.cube('cube %d' % i, w.material('cube%d' % i, color), -4 + i * 4, 0, 0, (0.3, 0.6, 0))
script = """'use strict';

export function update(value) {
	thisScene.setCameraTransforms({eye: new Vec3(0, 0, 6), center: new Vec3(0, 0, 0), up: new Vec3(0, 1, 0)});
	return value;
}
"""
w.add({"name": "camera driver", "image": "models/util/solidlayer.json", "size": "10 10", "color": "0 0 0", "alpha": 0,
       "origin": {"value": "10 10 0", "script": script, "scriptproperties": {}}, "angles": "0 0 0",
       "scale": "1 1 1", "alignment": "center"})
w.save()

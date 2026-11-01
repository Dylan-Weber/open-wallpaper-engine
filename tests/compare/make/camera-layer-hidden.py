# camera-layer-hidden: a script moves the camera once while a camera layer that is never shown sits on it
# (Ocarina of Time's "Look At Events" layers are hidden unless that camera type is chosen, and its manual camera
# script sets the camera each frame). The hidden layer must not take the camera back (owe #14).
w = Scene3D('camera-layer-hidden', 'a script moves the camera once, under a hidden camera layer')
for i, color in enumerate(('1 0.3 0.3', '0.3 1 0.3', '0.3 0.3 1')):
    w.cube('cube %d' % i, w.material('cube%d' % i, color), -4 + i * 4, 0, 0, (0.3, 0.6, 0))
w.add({"name": "cam hidden", "camera": "default", "origin": "0 0 12", "angles": "0 0 0", "fov": 50, "zoom": 1,
       "visible": False})
script = """'use strict';

export function init() {
	thisScene.setCameraTransforms({eye: new Vec3(0, 0, 6), center: new Vec3(0, 0, 0), up: new Vec3(0, 1, 0)});
}
"""
w.add({"name": "camera driver", "image": "models/util/solidlayer.json", "size": "10 10", "color": "0 0 0", "alpha": 0,
       "origin": {"value": "10 10 0", "script": script, "scriptproperties": {}}, "angles": "0 0 0",
       "scale": "1 1 1", "alignment": "center"})
w.save()

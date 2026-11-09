# camera-layer-restore: a camera layer drives the camera from close to the cubes, and a script hides it as the scene
# starts. Hiding it gives the camera back to the scene's own look-at camera (owe #23); a camera that went to the 2D
# camera's place instead shows the cubes from somewhere else.
w = Scene3D('camera-layer-restore', 'a script hides the camera layer that drives the camera')
for i, color in enumerate(('1 0.3 0.3', '0.3 1 0.3', '0.3 0.3 1')):
    w.cube('cube %d' % i, w.material('cube%d' % i, color), -4 + i * 4, 0, 0, (0.3, 0.6, 0))
w.add({"name": "cam close", "camera": "default", "origin": "0 0 5", "angles": "0 0 0", "fov": 50, "zoom": 1})
script = """'use strict';

export function init() {
	thisScene.getLayer('cam close').visible = false;
}
"""
w.add({"name": "hider", "image": "models/util/solidlayer.json", "size": "10 10", "color": "0 0 0", "alpha": 0,
       "origin": {"value": "10 10 0", "script": script, "scriptproperties": {}}, "angles": "0 0 0",
       "scale": "1 1 1", "alignment": "center"})
w.save()

# camera-script-read: a camera path holds the camera 3 units to the side; a cube's origin script puts it where
# thisScene.getCameraTransforms() says the camera is. Wallpaper Engine's scripts see the scene's own camera (eye 0 0 12),
# not where its path has moved the view, so its cube stays at 0 and shows left of the middle.
w = Scene3D('camera-script-read', 'a script reads the camera while a camera path moves it')
w.cube('still', w.material('white', '1 1 1'), 0, 2.5, 0, scale='0.5 0.5 0.5')
script = """'use strict';

export function update(value) {
	const eye = thisScene.getCameraTransforms().eye;
	return new Vec3(eye.x, value.y, value.z);
}
"""
w.add({"name": "follower", "model": w.material('red', '1 0.2 0.2'), "origin": {"value": "0 0 0", "script": script,
       "scriptproperties": {}}, "angles": "0.3 0.6 0", "scale": "1 1 1"})


def key(x, timestamp):
    return {"eye": vec(x, 0, 12), "center": vec(x, 0, 0), "up": "0 1 0", "timestamp": timestamp}


w.camera_paths = [{"duration": 60, "name": "", "transforms": [key(3, 0), key(3, 60)]}]
w.save()

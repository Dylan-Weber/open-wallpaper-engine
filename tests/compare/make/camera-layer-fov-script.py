# camera-layer-fov-script: the camera layer of camera-layer-fov (field of view 40 in a scene of 80) with a script on its
# angles that copies the scene's field of view into it in init (thisLayer.fov = thisScene.fov), as More Physics'
# camera layer does: Wallpaper Engine then draws the view with 80, like camera-layer-fov-same.
w = Scene3D('camera-layer-fov-script', 'a camera layer that copies the scene field of view in init')
w.general['fov'] = 80
for i, color in enumerate(('1 0.3 0.3', '0.3 1 0.3', '0.3 0.3 1')):
    w.cube('cube %d' % i, w.material('cube%d' % i, color), -4 + i * 4, 0, 0, (0.3, 0.6, 0))
script = """'use strict';
export function init() {
	thisLayer.fov = thisScene.fov;
}
export function update(value) {
	return value;
}
"""
w.add({"name": "cam", "camera": "default", "origin": "0 0 6", "angles": {"value": "0 0 0", "script": script,
                                                                      "scriptproperties": {}},
       "fov": 40, "zoom": 1})
w.save()
with open(os.path.join(HERE, 'camera-layer-fov-script', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'camera-layer-fov-script'))

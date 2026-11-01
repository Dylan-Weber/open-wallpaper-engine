# camera-layer-script: a camera layer that starts hidden and whose visible script says it should be shown drives the
# camera: the cubes are seen from close up (owe #24). Ocarina of Time's scripts pick which camera layer is shown.
w = Scene3D('camera-layer-script', 'a camera layer shown by its visible script')
for i, color in enumerate(('1 0.3 0.3', '0.3 1 0.3', '0.3 0.3 1')):
    w.cube('cube %d' % i, w.material('cube%d' % i, color), -4 + i * 4, 0, 0, (0.3, 0.6, 0))
script = """'use strict';

export function update(value) {
	return true;
}
"""
w.add({"name": "cam close", "camera": "default", "origin": "0 0 6", "angles": "0 0 0", "fov": 50, "zoom": 1,
       "visible": {"value": False, "script": script, "scriptproperties": {}}})
w.save()

# zz-cam-init2: setCameraTransforms from init(value) that returns the value
import math
w = Scene3D('zz-cam-init2', 'camera set from init with a return')
for i, color in enumerate(('1 0.3 0.3', '0.3 1 0.3', '0.3 0.3 1')):
    w.cube('cube %d' % i, w.material('cube%d' % i, color), -4 + i * 4, 0, 0, (0.3, 0.6, 0))
script = """'use strict';

export function init(value) {
	thisScene.setCameraTransforms({eye: new Vec3(0, 0, 6), center: new Vec3(0, 0, 0), up: new Vec3(0, 1, 0)});
	return value;
}
"""
w.add({"name": "camera driver", "image": "models/util/solidlayer.json", "size": "10 10", "color": "0 0 0", "alpha": 0,
       "origin": {"value": "10 10 0", "script": script, "scriptproperties": {}}, "angles": "0 0 0",
       "scale": "1 1 1", "alignment": "center"})
w.save()

# script-model-angles: a model's angles set by a script: the scene's 0.6 rad turn about y, a script returning 34.38 (the same
# in degrees), one returning 0.6, and one turning its model to face the camera's eye as OoT's interface does
# (atan2(eye.x - x, eye.z - z) in degrees). OoT's interface panel faces the camera in Wallpaper Engine and is not seen in owe.
w = Scene3D('script-model-angles', "models turned by scripts", ambient='1 1 1')
colors = ['1 0 0', '0 1 0', '0 0 1', '1 1 0']
fixed = """'use strict';
export function update(value) { return new Vec3(0, %s, 0); }
"""
facing = """'use strict';
export function update(value) {
    const eye = thisScene.getCameraTransforms().eye;
    const angle = Math.atan2(eye.x - thisLayer.origin.x, eye.z - thisLayer.origin.z) * (180 / Math.PI);
    return new Vec3(0, angle + 30, 0);
}
"""
for i, (title, angles) in enumerate((('scene 0.6 rad', (0, 0.6, 0)), ('script 34.38', fixed % '34.38'),
                                      ('script 0.6', fixed % '0.6'), ('facing the eye, plus 30', facing))):
    mdl = w.material('cube%d' % i, colors[i])
    cube = w.cube(title, mdl, -4.5 + i * 3, 0, 0)
    if not isinstance(angles, tuple):
        w.objects[-1]["angles"] = {"value": "0 0 0", "script": angles, "scriptproperties": {}}
    else:
        w.objects[-1]["angles"] = vec(*angles)
w.save()
with open(os.path.join(HERE, 'script-model-angles', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'script-model-angles'))

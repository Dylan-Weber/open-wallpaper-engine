# general-fov-script: a script on the scene's general fov (Ocarina of Time zooms its camera with one, and sets
# shared.zooming at its top level, which its interface's scripts read). Its update returns 30 (the scene's fov is 50),
# and its top level sets shared.fovscript = 1; a red cube's origin script puts it above the white
# cube when that is set, below it when not. owe never ran general fov scripts: no zoom, and the red cube below.
w = Scene3D('general-fov-script', "a script on the scene's fov")
w.cube('cube', w.material('white', '1 1 1'), 0, 0, 0)
red = w.material('red', '1 0.2 0.2')
w.add({"name": "marker", "model": red, "angles": "0 0 0", "scale": "0.3 0.3 0.3",
       "origin": {"value": "0 -2 0", "scriptproperties": {}, "script": """'use strict';
export function update(value) {
    return new Vec3(0, shared.fovscript === 1 ? 2 : -2, 0);
}
"""}})
w.general['fov'] = {"value": 50, "script": """'use strict';
shared.fovscript = 1;
export function update(value) {
    return 30;
}
"""}
w.save()
with open(os.path.join(HERE, 'general-fov-script', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'general-fov-script'))

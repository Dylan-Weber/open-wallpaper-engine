# script-workshop-path: thisScene.createLayer('models/sq.json') in a script that says `export let __workshopId = '123'`,
# where the model is only at models/workshop/123/sq.json (as Reze's audio visualizer asks for 'models/bar.json', which its
# package has only at models/workshop/2652493753/bar.json). Squares: created by the workshop path, by the short path from a
# script with the id, by the short path from one without it; the model drawn as a layer of the scene.
w = Wallpaper('script-workshop-path', "createLayer and a workshop script's asset paths", clear='0.05 0.05 0.1')
w.files['models/workshop/123/sq.json'] = {"autosize": True, "material": "materials/workshop/123/sq.json"}
w.files['materials/workshop/123/sq.json'] = {"passes": [{"blending": "normal", "cullmode": "nocull", "depthtest": "disabled",
                                                         "depthwrite": "disabled", "shader": "genericimage2",
                                                         "textures": ["gradient/gradient_rainbow"]}]}
def creator(x, path, workshop):
    return """'use strict';
%sexport function init(value) {
    let made = thisScene.createLayer('%s');
    made.origin = new Vec3(%d, 540, 0);
    made.scale = new Vec3(1, 1, 1);
    return value;
}
export function update(value) { return value; }
""" % ("export let __workshopId = '123';\n" if workshop else '', path, x)
for name, x, path, workshop in (('workshop path', 300, 'models/workshop/123/sq.json', False),
                                ('short path, with the id', 750, 'models/sq.json', True),
                                ('short path, without it', 1200, 'models/sq.json', False)):
    w.add({"name": name, "image": "models/util/solidlayer.json", "size": "10 10", "color": "0 0 0", "alpha": 0,
           "origin": {"value": "%d 900 0" % x, "script": creator(x, path, workshop), "scriptproperties": {}},
           "angles": "0 0 0", "scale": "1 1 1", "alignment": "center"})
w.add({"name": "in the scene", "image": "models/workshop/123/sq.json", "origin": "1650 540 0", "size": "256 256",
       "color": "1 1 1", "alpha": 1, "angles": "0 0 0", "scale": "1 1 1", "alignment": "center"})
w.save()
with open(os.path.join(HERE, 'script-workshop-path', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'script-workshop-path'))

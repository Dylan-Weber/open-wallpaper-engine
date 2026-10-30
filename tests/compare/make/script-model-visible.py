# script-model-visible: visible scripts on models (not image layers): returning false, returning true, and turning
# false after two seconds. In general-fov-script's first version owe showed a model whose visible script returned
# false.
w = Scene3D('script-model-visible', 'visible scripts on models')
red = w.material('red', '1 0.2 0.2')
green = w.material('green', '0.2 1 0.2')
blue = w.material('blue', '0.3 0.4 1')


def visible(script):
    return {"value": True, "scriptproperties": {}, "script": "'use strict';\nexport function update(value) {\n" + script + "\n}\n"}


w.add({"name": "returns false", "model": red, "origin": "-3 0 0", "angles": "0 0.5 0", "scale": "1 1 1",
       "visible": visible("    return false;")})
w.add({"name": "returns true", "model": green, "origin": "0 0 0", "angles": "0 0.5 0", "scale": "1 1 1",
       "visible": visible("    return true;")})
w.add({"name": "false after two seconds", "model": blue, "origin": "3 0 0", "angles": "0 0.5 0", "scale": "1 1 1",
       "visible": visible("    return engine.runtime < 2;")})
w.save()
with open(os.path.join(HERE, 'script-model-visible', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'script-model-visible'))


# script-layer-visible: the same three visible scripts on 2D image layers
w = Wallpaper('script-layer-visible', 'visible scripts on image layers', clear='0.05 0.05 0.1')
for name, x, color, script in (('returns false', 560, '1 0.2 0.2', "    return false;"),
                               ('returns true', 960, '0.2 1 0.2', "    return true;"),
                               ('false after two seconds', 1360, '0.3 0.4 1', "    return engine.runtime < 2;")):
    w.add({"name": name, "image": "models/util/solidlayer.json", "size": "250 250", "color": color, "alpha": 1,
           "origin": "%d 540 0" % x, "angles": "0 0 0", "scale": "1 1 1", "alignment": "center",
           "visible": visible(script)})
w.save()
with open(os.path.join(HERE, 'script-layer-visible', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'script-layer-visible'))

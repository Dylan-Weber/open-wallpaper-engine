# particles-layerimage: the layerimage emitter, which emits from the pixels of a (hidden) image layer the particle object
# depends on (type emitterimage). Dense: 16000 particles of size 40 on 400x200 units, so the picture is the layer's shape.
# Cells: a white rectangle 400x200; a red one 300x150; a 300x100 one turned 30 degrees; a 400x200 one with opacity 0.3.
def particle(maxcount=16000):
    return {
        "material": "materials/particle/halo.json", "maxcount": maxcount, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"}],
        "emitter": [{"id": 1, "name": "layerimage", "rate": 16000}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 100000, "max": 100000},
            {"id": 3, "name": "sizerandom", "min": 40, "max": 40},
        ],
        "operator": [],
        "renderer": [{"id": 90, "name": "sprite"}],
    }


cells = (
    ("white", 400, 200, "1 1 1", 1, 0),
    ("red", 300, 150, "1 0 0", 1, 0),
    ("turned", 300, 100, "1 1 1", 1, 30),
    ("opacity 0.3", 400, 200, "1 1 1", 0.3, 0),
)
w = Wallpaper('particles-layerimage', 'particles emitted from a layer\'s pixels', clear='0 0 0')
w.files['particles/layer.json'] = particle()
for k, (name, width, height, color, alpha, angle) in enumerate(cells):
    x, y = 480 + 960 * (k % 2), 760 - 480 * (k // 2)
    layer = w.solid(name, x, y, width, height, color, alpha, angle, visible=False)
    w.add({"name": "particles " + name, "particle": "particles/layer.json", "origin": "%d %d 0" % (x, y),
           "scale": "1 1 1", "angles": "0 0 0", "visible": True,
           "dependencies": [{"id": layer, "index": 0, "type": "emitterimage"}]})
w.save()
clock_probe.stamp(os.path.join(HERE, 'particles-layerimage'))

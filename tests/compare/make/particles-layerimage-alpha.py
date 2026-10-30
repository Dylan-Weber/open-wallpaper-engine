# particles-layerimage: the layerimage emitter, which emits from the pixels of a (hidden) image layer the particle object
# depends on (type emitterimage). Dense: 3000 particles of size 24 on 400x200 units, so the picture is the layer's shape.
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
    ("alpha 0.5", 300, 150, "1 1 1", 0.5, 0),
    ("alpha 0.7", 300, 150, "1 1 1", 0.7, 0),
    ("alpha 0.9", 300, 150, "1 1 1", 0.9, 0),
)
w = Wallpaper('particles-layerimage-alpha', 'layerimage emission and the layer\'s opacity, and textures', clear='0 0 0')
w.files['particles/layer.json'] = particle()
for k, (name, width, height, color, alpha, angle) in enumerate(cells):
    x, y = 340 + 640 * (k % 3), 800
    layer = w.solid(name, x, y, width, height, color, alpha, angle, visible=False)
    w.add({"name": "particles " + name, "particle": "particles/layer.json", "origin": "%d %d 0" % (x, y),
           "scale": "1 1 1", "angles": "0 0 0", "visible": True,
           "dependencies": [{"id": layer, "index": 0, "type": "emitterimage"}]})
# textures: the soft round halo (alpha falls off), the rainbow gradient (colours)
for k, (name, texture) in enumerate((("halo", "particle/halo"), ("rainbow", "gradient/gradient_rainbow"))):
    model = w.texture_model(name, texture)
    x, y = 340 + 640 * k, 420
    layer = w.image(name, model, x, y, 300, 300, visible=False)
    w.add({"name": "particles " + name, "particle": "particles/layer.json", "origin": "%d %d 0" % (x, y),
           "scale": "1 1 1", "angles": "0 0 0", "visible": True,
           "dependencies": [{"id": layer, "index": 0, "type": "emitterimage"}]})
w.save()
clock_probe.stamp(os.path.join(HERE, 'particles-layerimage-alpha'))

# particles-layerimage-color: a layerimage emitter's pixel colours and a colorrandom initializer (multiply, set or ignored?).
# Dense particles from the rainbow gradient (colours): no colorrandom; white; red; mid grey.
def particle(extra):
    return {
        "material": "materials/particle/halo.json", "maxcount": 16000, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"}],
        "emitter": [{"id": 1, "name": "layerimage", "rate": 16000}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 100000, "max": 100000},
            {"id": 3, "name": "sizerandom", "min": 24, "max": 24},
        ] + extra,
        "operator": [],
        "renderer": [{"id": 90, "name": "sprite"}],
    }


cells = (
    ("none", []),
    ("white", [{"id": 4, "name": "colorrandom", "min": "255 255 255", "max": "255 255 255"}]),
    ("red", [{"id": 4, "name": "colorrandom", "min": "255 0 0", "max": "255 0 0"}]),
    ("grey", [{"id": 4, "name": "colorrandom", "min": "128 128 128", "max": "128 128 128"}]),
)
w = Wallpaper('particles-layerimage-color', 'layerimage pixel colours and colorrandom', clear='0 0 0')
for k, (name, extra) in enumerate(cells):
    w.files['particles/c%d.json' % k] = particle(extra)
    model = w.texture_model(name, "gradient/gradient_rainbow")
    x, y = 480 + 960 * (k % 2), 760 - 480 * (k // 2)
    layer = w.image(name, model, x, y, 300, 300, visible=False)
    w.add({"name": "particles " + name, "particle": "particles/c%d.json" % k, "origin": "%d %d 0" % (x, y),
           "scale": "1 1 1", "angles": "0 0 0", "visible": True,
           "dependencies": [{"id": layer, "index": 0, "type": "emitterimage"}]})
w.save()
clock_probe.stamp(os.path.join(HERE, 'particles-layerimage-color'))

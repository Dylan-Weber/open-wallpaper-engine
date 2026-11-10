# particles-layerimage-edge2: layerimage points for textures exactly 4 high or wide, and 4-7 on both sides (the grid's
# 2-texel minimum and the 4-texel steps; particles-layerimage-edge has the thin sizes under 4 and 5-9 against 32).
# 480x270 scene, tiny particles, white images in 3 rows of 6, 75 units apart.
def particle(maxcount, size):
    return {
        "material": "materials/particle/halo.json", "maxcount": maxcount, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"}],
        "emitter": [{"id": 1, "name": "layerimage", "rate": 100000}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 100000, "max": 100000},
            {"id": 3, "name": "sizerandom", "min": size, "max": size},
        ],
        "operator": [],
        "renderer": [{"id": 90, "name": "sprite"}],
    }


def white(width, height):
    return bytes((255, 255, 255, 255)) * (width * height)


sizes = [(32, 4), (4, 32), (4, 4), (64, 4), (4, 64), (8, 4), (4, 8), (5, 5), (6, 6), (7, 7), (5, 7), (7, 5), (6, 4),
         (4, 6), (12, 4), (4, 12), (7, 32), (32, 7)]
w = Wallpaper('particles-layerimage-edge2', 'layerimage points at 4 high or wide and 5-7', clear='0 0 0', size=(480, 270))
for k, (tw, th) in enumerate(sizes):
    x, y = 40 + 75 * (k % 6), 225 - 75 * (k // 6)
    w.files['particles/c%d.json' % k] = particle(20000, 0.8)
    model = w.texture_model('img%d' % k, tex_file(w, 's%dx%d' % (tw, th), tw, th, white(tw, th)))
    layer = w.image('img%d' % k, model, x, y, tw, th, visible=False)
    w.add({"name": "edge %d" % k, "particle": "particles/c%d.json" % k, "origin": "%d %d 0" % (x, y),
           "scale": "1 1 1", "angles": "0 0 0", "visible": True,
           "dependencies": [{"id": layer, "index": 0, "type": "emitterimage"}]})
w.save()
clock_probe.stamp(os.path.join(HERE, 'particles-layerimage-edge2'))
with open(os.path.join(HERE, 'particles-layerimage-edge2', 'compare.json'), 'w') as f:
    json.dump({"probe": True, "fps": 30}, f)

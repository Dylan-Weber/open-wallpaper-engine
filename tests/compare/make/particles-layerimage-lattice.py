# particles-layerimage-lattice: where a layerimage emitter puts particles. Wallpaper Engine puts them on a regular lattice of
# the image's pixels (every few pixels, no jitter), not at random. Cells: an opaque white image of 32, 64, 128 and 256 pixels
# (10000 particles), and a 256-pixel one with 1000, 3000, 30000 and 60000 particles. Tiny particles (size 3) so the lattice shows.
def particle(maxcount):
    return {
        "material": "materials/particle/halo.json", "maxcount": maxcount, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"}],
        "emitter": [{"id": 1, "name": "layerimage", "rate": 100000}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 100000, "max": 100000},
            {"id": 3, "name": "sizerandom", "min": 3, "max": 3},
        ],
        "operator": [],
        "renderer": [{"id": 90, "name": "sprite"}],
    }


cells = ((32, 10000), (64, 10000), (128, 10000), (256, 10000), (256, 1000), (256, 3000), (256, 30000), (256, 60000),
         (128, 1000), (128, 3000), (128, 30000), (64, 1000))
w = Wallpaper('particles-layerimage-lattice', 'layerimage emission lattice', clear='0 0 0')
for k, (size, count) in enumerate(cells):
    w.files['particles/c%d.json' % k] = particle(count)
    name = tex_file(w, 'white%d' % size, size, size, bytes((255, 255, 255, 255)) * (size * size))
    model = w.texture_model('img%d' % k, name)
    x, y = 280 + 440 * (k % 4), 860 - 330 * (k // 4)
    layer = w.image('img%d' % k, model, x, y, size, size, visible=False)
    w.add({"name": "lattice %d" % k, "particle": "particles/c%d.json" % k, "origin": "%d %d 0" % (x, y),
           "scale": "1 1 1", "angles": "0 0 0", "visible": True,
           "dependencies": [{"id": layer, "index": 0, "type": "emitterimage"}]})
w.save()
clock_probe.stamp(os.path.join(HERE, 'particles-layerimage-lattice'))
with open(os.path.join(HERE, 'particles-layerimage-lattice', 'compare.json'), 'w') as f:
    json.dump({"probe": True, "fps": 30}, f)

# particles-override-brightness: a particle layer's instanceoverride "brightness" (thunderbolt's preset sets 5). One halo
# particle at rest per layer, colour 0.5 0.6 0.8, alpha 0.1 or 1, with brightness 1, 2 and 5, on the additive halo
# material and on the translucent one.
def cell(alpha, material):
    return {
        "material": material, "maxcount": 1, "starttime": 0, "flags": 0,
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "0 0 0", "rate": 0, "instantaneous": 1,
                     "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 100000, "max": 100000},
            {"id": 3, "name": "sizerandom", "min": 160, "max": 160},
            {"id": 4, "name": "colorrandom", "min": "127 153 204", "max": "127 153 204"},
            {"id": 5, "name": "alpharandom", "min": alpha, "max": alpha},
        ],
        "operator": [{"id": 6, "name": "movement", "drag": 0}],
        "renderer": [{"id": 90, "name": "sprite"}],
    }


name = 'particles-override-brightness'
w = Wallpaper(name, 'instanceoverride brightness on particles', clear='0 0 0')
k = 0
for row, (alpha, material) in enumerate(((0.1, 'materials/particle/halo.json'), (1.0, 'materials/particle/halo.json'),
                                          (0.1, 'materials/particle/halo_translucent.json'))):
    w.files['particles/r%d.json' % row] = cell(alpha, material)
    for col, brightness in enumerate((None, 1, 2, 5)):
        layer = {"name": "a%g b%s r%d" % (alpha, brightness, row), "particle": "particles/r%d.json" % row,
                 "origin": "%d %d 0" % (300 + 440 * col, 860 - 300 * row), "scale": "1 1 1", "angles": "0 0 0"}
        if brightness is not None:
            layer["instanceoverride"] = {"brightness": float(brightness)}
        w.add(layer)
w.save()
clock_probe.stamp(os.path.join(HERE, name))
write(name + '/compare.json', {"probe": True, "fps": 30})

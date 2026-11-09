# particles-no-renderer: a particle system whose "renderer" list is empty (thunderbolt's child spawner), one without the key,
# and one with a sprite renderer. One halo particle at rest each.
def cell(renderer):
    system = {
        "material": "materials/particle/halo.json", "maxcount": 1, "starttime": 0, "flags": 0,
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "0 0 0", "rate": 0, "instantaneous": 1,
                     "speedmin": 0, "speedmax": 0}],
        "initializer": [{"id": 2, "name": "lifetimerandom", "min": 100000, "max": 100000},
                        {"id": 3, "name": "sizerandom", "min": 120, "max": 120}],
        "operator": [{"id": 5, "name": "movement", "drag": 0}],
    }
    if renderer is not None:
        system["renderer"] = renderer
    return system


cells = (("empty list", []), ("no key", None), ("sprite", [{"id": 90, "name": "sprite"}]))
name = 'particles-no-renderer'
w = Wallpaper(name, 'particle systems without a renderer', clear='0 0 0')
for k, (label, renderer) in enumerate(cells):
    w.files['particles/c%d.json' % k] = cell(renderer)
    w.add({"name": label, "particle": "particles/c%d.json" % k,
           "origin": "%d 600 0" % (400 + 560 * k), "scale": "1 1 1", "angles": "0 0 0"})
w.save()
clock_probe.stamp(os.path.join(HERE, name))
write(name + '/compare.json', {"probe": True, "fps": 30})

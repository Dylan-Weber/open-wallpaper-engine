# particles-cp-worldspace: a worldspace control point's own offset (no instanceoverride), as thunderbolt's control
# point 1 ("flags": 2, "offset": "0 -450 0"). Six particles at rest are spaced from control point 0 to 1
# (mapsequencebetweencontrolpoints), so the dots show where control point 1 is. Layers at three places with the same
# worldspace offset (960 540: the middle of the picture), one at a scale of 2, and one without the flag (an offset
# from the layer) for comparison.
def cell(flags):
    return {
        "material": "materials/particle/halo.json", "maxcount": 6, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"},
                         {"id": 1, "flags": flags, "offset": "960 540 0"}],
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "0 0 0", "rate": 0, "instantaneous": 6,
                     "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 100000, "max": 100000},
            {"id": 3, "name": "sizerandom", "min": 24, "max": 24},
            {"id": 4, "name": "mapsequencebetweencontrolpoints", "count": 6},
        ],
        "operator": [{"id": 5, "name": "movement", "drag": 0}],
        "renderer": [{"id": 90, "name": "sprite"}],
    }


name = 'particles-cp-worldspace'
w = Wallpaper(name, 'a worldspace control point offset without an override', clear='0 0 0')
w.files['particles/world.json'] = cell(2)
w.files['particles/local.json'] = cell(0)
for k, (x, y, scale, particle) in enumerate(((300, 900, 1, 'world'), (1600, 850, 1, 'world'),
                                             (1500, 300, 1, 'world'), (400, 350, 2, 'world'),
                                             (200, 200, 1, 'local'))):
    w.add({"name": "%s %d" % (particle, k), "particle": "particles/%s.json" % particle,
           "origin": "%d %d 0" % (x, y), "scale": "%g %g 1" % (scale, scale), "angles": "0 0 0"})
w.save()
clock_probe.stamp(os.path.join(HERE, name))
write(name + '/compare.json', {"probe": True, "fps": 30})

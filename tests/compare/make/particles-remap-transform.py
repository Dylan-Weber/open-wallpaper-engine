# particles-remap-transform: remapvalue's transformfunction (triangle, sine, square) over a particle's lifetime fraction at
# several points. One particle per cell, grey = the transformed fraction (colour starts at white; set); the particles'
# lifetimes are chosen so the fraction at the capture (~11.3 s) is 0.125, 0.25, 0.375, 0.5, 0.75 and 0.94.
def cell(lifetime, function):
    return {
        "material": "materials/particle/halo.json", "maxcount": 4, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"}],
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "0 0 0", "rate": 0, "instantaneous": 1,
                     "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": lifetime, "max": lifetime},
            {"id": 3, "name": "sizerandom", "min": 60, "max": 60},
        ],
        "operator": [{"id": 6, "name": "remapvalue", "input": "lifetimefraction", "inputrangemin": 0,
                      "inputrangemax": 1, "output": "color", "operation": "remap", "transformfunction": function,
                      "outputrangemin": "0 0 0", "outputrangemax": "1 1 1"}],
        "renderer": [{"id": 90, "name": "sprite"}],
    }


lifetimes = (90, 45, 30, 22.5, 15, 12)
w = Wallpaper('particles-remap-transform', 'remapvalue transform functions', clear='0 0 0')
k = 0
for row, function in enumerate(("triangle", "sine", "square")):
    for column, lifetime in enumerate(lifetimes):
        w.files['particles/c%d.json' % k] = cell(lifetime, function)
        w.add({"name": "%s %s" % (function, lifetime), "particle": "particles/c%d.json" % k,
               "origin": "%d %d 0" % (180 + 300 * column, 850 - 250 * row), "scale": "1 1 1", "angles": "0 0 0",
               "visible": True})
        k += 1
w.save()
clock_probe.stamp(os.path.join(HERE, 'particles-remap-transform'))

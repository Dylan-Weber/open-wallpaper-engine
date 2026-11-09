# particles-cp-flag16: control point flag 16 (dripping_water's CP1 and CP2, placed by the preset's instanceoverride
# controlpoint1/2 "22 0 0" / "-22 0 0" and controlpointangle1/2). One particle at rest is emitted at control point 1
# (sphererandom "controlpoint": 1), so the dot shows where CP1 is. Cells: offset 100 0 with flags 0 and 16; an
# instanceoverride controlpoint1 100 0 with flags 0 and 16; flag 16 with the override and controlpointangle1 -0.5236; the
# same at layer scale 3.
def cell(flags, offset):
    return {
        "material": "materials/particle/halo.json", "maxcount": 1, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0}, dict({"id": 1, "flags": flags}, **({"offset": offset} if offset else {}))],
        "emitter": [{"id": 1, "name": "sphererandom", "controlpoint": 1, "distancemin": 0, "distancemax": 0,
                     "rate": 0, "instantaneous": 1}],
        "initializer": [{"id": 2, "name": "lifetimerandom", "min": 100000, "max": 100000},
                        {"id": 3, "name": "sizerandom", "min": 30, "max": 30}],
        "operator": [{"id": 5, "name": "movement", "drag": 0}],
        "renderer": [{"id": 90, "name": "sprite"}],
    }


cells = (
    ("offset, flags 0", cell(0, "100 0 0"), None, 1),
    ("offset, flags 16", cell(16, "100 0 0"), None, 1),
    ("override, flags 0", cell(0, None), {"controlpoint1": "100 0 0"}, 1),
    ("override, flags 16", cell(16, None), {"controlpoint1": "100 0 0"}, 1),
    ("override + angle, flags 16", cell(16, None), {"controlpoint1": "100 0 0", "controlpointangle1": "0 0 -0.5236"}, 1),
    ("override, flags 16, scale 3", cell(16, None), {"controlpoint1": "100 0 0"}, 3),
    ("override, flags 0, scale 3", cell(0, None), {"controlpoint1": "100 0 0"}, 3),
)
name = 'particles-cp-flag16'
w = Wallpaper(name, 'control point flag 16 with instanceoverride positions', clear='0 0 0')
for k, (label, system, override, scale) in enumerate(cells):
    w.files['particles/c%d.json' % k] = system
    layer = {"name": label, "particle": "particles/c%d.json" % k,
             "origin": "%d %d 0" % (300 + 500 * (k % 3), 860 - 300 * (k // 3)), "scale": "%d %d 1" % (scale, scale),
             "angles": "0 0 0"}
    if override:
        layer["instanceoverride"] = override
    w.add(layer)
# a marker dot at each layer's origin, to measure from
w.files['particles/marker.json'] = dict(cell(0, None), emitter=[{"id": 1, "name": "sphererandom", "distancemin": 0,
                                                                  "distancemax": 0, "rate": 0, "instantaneous": 1}],
                                         initializer=[{"id": 2, "name": "lifetimerandom", "min": 100000, "max": 100000},
                                                      {"id": 3, "name": "sizerandom", "min": 8, "max": 8}])
for k in range(len(cells)):
    w.add({"name": "marker %d" % k, "particle": "particles/marker.json",
           "origin": "%d %d 0" % (300 + 500 * (k % 3), 860 - 300 * (k // 3)), "scale": "1 1 1", "angles": "0 0 0"})
w.save()
clock_probe.stamp(os.path.join(HERE, name))
write(name + '/compare.json', {"probe": True, "fps": 30})

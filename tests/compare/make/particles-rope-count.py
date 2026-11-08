# particles-rope-count: ropes of 2, 3, 5 and 10 particles on the same line (400 units, between two control points),
# so the ribbon's ends show which particles it runs through, and one rope with the particles' sizes set apart.
def rope(count, size=40):
    return {
        "material": "materials/particle/halo.json", "maxcount": 20, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"}, {"id": 1, "flags": 0, "offset": "400 0 0"}],
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "0 0 0", "rate": 0, "instantaneous": count,
                     "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 100000, "max": 100000},
            {"id": 3, "name": "sizerandom", "min": size, "max": size},
            {"id": 4, "name": "colorrandom", "min": "255 200 80", "max": "255 200 80"},
            {"id": 5, "name": "mapsequencebetweencontrolpoints", "controlpointstart": 0, "controlpointend": 1,
             "count": count},
        ],
        "operator": [],
        "renderer": [{"id": 90, "name": "rope"}],
    }


w = Wallpaper('particles-rope-count', 'ropes through 2, 3, 5 and 10 particles', clear='0 0 0')
for k, count in enumerate((2, 3, 5, 10)):
    w.files['particles/r%d.json' % count] = rope(count)
    w.add({"name": "rope %d" % count, "particle": "particles/r%d.json" % count,
           "origin": "300 %d 0" % (880 - 220 * k), "scale": "1 1 1", "angles": "0 0 0", "visible": True})
w.save()
with open(os.path.join(HERE, 'particles-rope-count', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'particles-rope-count'))

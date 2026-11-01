# particles-offsetrandom: the positionoffsetrandom initializer scatters new particles: 300 particles emitted at one
# point with distance 150 (left) and 60 (right), no other movement; their spread shows its shape.
def burst(distance):
    return {
        "material": "materials/particle/halo.json", "maxcount": 320, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"}],
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "0 0 0", "rate": 0, "instantaneous": 300,
                     "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 100000, "max": 100000},
            {"id": 3, "name": "sizerandom", "min": 8, "max": 8},
            {"id": 4, "name": "positionoffsetrandom", "distance": distance},
        ],
        "operator": [],
        "renderer": [{"id": 90, "name": "sprite"}],
    }


w = Wallpaper('particles-offsetrandom', 'particles scattered by positionoffsetrandom', clear='0 0 0')
for k, distance in enumerate((150, 60)):
    w.files['particles/d%d.json' % k] = burst(distance)
    w.add({"name": "distance %d" % distance, "particle": "particles/d%d.json" % k,
           "origin": "%d 560 0" % (600 + 700 * k), "scale": "1 1 1", "angles": "0 0 0", "visible": True})
w.save()
clock_probe.stamp(os.path.join(HERE, 'particles-offsetrandom'))

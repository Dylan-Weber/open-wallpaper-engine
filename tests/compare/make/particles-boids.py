# particles-boids: the boids operator on small groups of particles that start at rest (or moving, for alignment) and move
# by the movement operator alone. Wallpaper Engine takes particles four at a time and the unused places of the last
# group count as particles at rest at the system's origin, so a pair is pushed apart from, and pulled towards, the
# point half way between them as well as each other.
def cell(points, sep_t=60, nb_t=200, sep=0, align=0, coh=0, velocity="0 0 0"):
    return {
        "material": "materials/particle/halo.json", "maxcount": 8, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"}],
        "emitter": [{"id": 10 + k, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "%g 0 0" % x, "rate": 0, "instantaneous": 1,
                     "speedmin": 0, "speedmax": 0} for k, x in enumerate(points)],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 100000, "max": 100000},
            {"id": 3, "name": "sizerandom", "min": 50, "max": 50},
            {"id": 4, "name": "velocityrandom", "min": velocity, "max": velocity},
        ],
        "operator": [
            {"id": 5, "name": "movement"},
            {"id": 6, "name": "boids", "separationthreshold": sep_t, "neighborthreshold": nb_t, "maxspeed": 100000,
             "separationfactor": sep, "alignmentfactor": align, "cohesionfactor": coh},
        ],
        "renderer": [{"id": 90, "name": "sprite"}],
    }


S, C = 0.01, 0.002
cells = [
    cell([-10, 10], sep=S), cell([-20, 20], sep=S), cell([-40, 40], sep=S), cell([-50, 50], sep=S),
    cell([-20, 20], sep_t=30, sep=S), cell([-30, 0, 30], sep=S), cell([-25, 25], nb_t=100, coh=C),
    cell([-55, 55], nb_t=100, coh=C), cell([-125, 125], coh=C), cell([-60, -40, 60], coh=C),
    cell([-60, 60], sep=S, coh=C), cell([0], align=0.1, velocity="30 0 0"),
]
w = Wallpaper('particles-boids', 'the boids operator on small groups of particles', clear='0 0 0')
for k, c in enumerate(cells):
    w.files['particles/c%d.json' % k] = c
    w.add({"name": "boids %d" % k, "particle": "particles/c%d.json" % k,
           "origin": "%d %d 0" % (300 + 440 * (k % 4), 870 - 250 * (k // 4)), "scale": "1 1 1", "angles": "0 0 0",
           "visible": True})
w.save()
clock_probe.stamp(os.path.join(HERE, 'particles-boids'))

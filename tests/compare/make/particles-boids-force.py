# particles-boids-force: the force the boids operator gives two particles at rest. No movement operator, so the particles keep
# their places and the force is constant; their velocity then grows by (force x time). Each particle shows its velocity as
# a colour: remapvalue maps `velocity` (per axis, -R..R) onto 0..1, so the colour's red is (vx + R) / 2R. Two particles per
# cell, `d` apart on the x axis.
R = 1000


def cell(d, separationthreshold, neighborthreshold, separation, alignment, cohesion, maxspeed=100000):
    emitter = lambda i, x: {"id": i, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                            "directions": "1 1 0", "origin": "%g 0 0" % x, "rate": 0, "instantaneous": 1,
                            "speedmin": 0, "speedmax": 0}
    return {
        "material": "materials/particle/halo.json", "maxcount": 4, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"}],
        "emitter": [emitter(1, -d / 2.0), emitter(10, d / 2.0)],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 100000, "max": 100000},
            {"id": 3, "name": "sizerandom", "min": 30, "max": 30},
        ],
        "operator": [
            {"id": 5, "name": "boids", "separationthreshold": separationthreshold,
             "neighborthreshold": neighborthreshold, "maxspeed": maxspeed, "separationfactor": separation,
             "alignmentfactor": alignment, "cohesionfactor": cohesion},
            {"id": 6, "name": "remapvalue", "input": "velocity", "inputrangemin": -R, "inputrangemax": R,
             "output": "color", "operation": "remap", "outputrangemin": "0 0 0", "outputrangemax": "1 1 1"},
        ],
        "renderer": [{"id": 90, "name": "sprite"}],
    }


# d, separation threshold, neighbour threshold, separation, alignment, cohesion
ds = (5, 10, 15, 20, 30, 40, 50, 60, 70, 80, 100, 120)
# the first 12 cells (2 captures): separation at distances, threshold 60 (variant A) or 30 (variant B)
import os as _os
threshold = float(_os.environ.get('BOIDS_SEP', '60'))
cells = tuple((d, threshold, 200, 1, 0, 0) for d in ds)
w = Wallpaper('particles-boids-force', 'boids force on two particles at rest', clear='0 0 0')
for k, args in enumerate(cells):
    w.files['particles/c%d.json' % k] = cell(*args)
    w.add({"name": "boids %d" % k, "particle": "particles/c%d.json" % k,
           "origin": "%d %d 0" % (340 + 440 * (k % 4), 850 - 250 * (k // 4)), "scale": "1 1 1", "angles": "0 0 0",
           "visible": True})
w.save()
clock_probe.stamp(os.path.join(HERE, 'particles-boids-force'))

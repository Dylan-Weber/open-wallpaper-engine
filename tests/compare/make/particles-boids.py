# particles-boids: the boids operator. Two particles are placed at fixed spots (mapsequencebetweencontrolpoints between
# control points 0 and 1), at rest, with drag 3 so they come to rest; the gap they end with shows the forces. Cells:
# separation only (spacing 20), separation with separationthreshold 60, cohesion only (spacing 100, neighborthreshold 150),
# cohesion with neighborthreshold 50 (out of reach), then both at once, then three particles with cohesion.
def cell(spacing, boids, count=2):
    half = spacing / 2.0
    return {
        "material": "materials/particle/halo.json", "maxcount": 8, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "%g 0 0" % -half}, {"id": 1, "flags": 0, "offset": "%g 0 0" % half}],
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "0 0 0", "rate": 0, "instantaneous": count,
                     "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 100000, "max": 100000},
            {"id": 3, "name": "sizerandom", "min": 30, "max": 30},
            {"id": 4, "name": "mapsequencebetweencontrolpoints", "count": count},
        ],
        "operator": [{"id": 5, "name": "movement", "drag": 3}, dict({"id": 6, "name": "boids"}, **boids)],
        "renderer": [{"id": 90, "name": "sprite"}],
    }


zero = {"cohesionfactor": 0, "separationfactor": 0, "alignmentfactor": 0}
cells = (
    ("separation", 20, dict(zero, separationfactor=25), 2),
    ("separation, threshold 60", 20, dict(zero, separationfactor=25, separationthreshold=60), 2),
    ("cohesion", 100, dict(zero, cohesionfactor=4, neighborthreshold=150), 2),
    ("cohesion, out of reach", 100, dict(zero, cohesionfactor=4, neighborthreshold=50), 2),
    ("both", 60, dict(cohesionfactor=4, separationfactor=25, neighborthreshold=100, alignmentfactor=0), 2),
    ("three, cohesion", 120, dict(zero, cohesionfactor=4, neighborthreshold=150), 3),
)
w = Wallpaper('particles-boids', 'boids forces on particles at rest', clear='0 0 0')
for k, (name, spacing, boids, count) in enumerate(cells):
    w.files['particles/c%d.json' % k] = cell(spacing, boids, count)
    w.add({"name": name, "particle": "particles/c%d.json" % k,
           "origin": "%d %d 0" % (320 + 640 * (k % 3), 750 - 400 * (k // 3)), "scale": "1 1 1", "angles": "0 0 0",
           "visible": True})
w.save()
clock_probe.stamp(os.path.join(HERE, 'particles-boids'))

# particles-collisionshapes: collisionsphere (top row) and collisionquad (bottom row) with the three behaviours.
# A stream thrown right at 40 and up at 200 from 150 above the shape's centre, gravity -500, so it falls on the shape.
def cell(operator):
    return {
        "material": "materials/particle/halo.json", "maxcount": 200, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"}],
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "-60 100 0", "rate": 20, "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 4, "max": 4},
            {"id": 3, "name": "sizerandom", "min": 24, "max": 24},
            {"id": 4, "name": "velocityrandom", "min": "40 200 0", "max": "40 200 0"},
        ],
        "operator": [{"id": 5, "name": "movement", "gravity": "0 -500 0", "drag": 0}, dict(operator, id=6)],
        "renderer": [{"id": 90, "name": "sprite"}],
    }


cells = []
for behavior in ("bounce", "slide", "delete"):
    cells.append({"name": "collisionsphere", "origin": "0 -100 0", "radius": 100, "collisionbehavior": behavior,
                  "bouncefactor": 0.7})
for behavior in ("bounce", "slide", "delete"):
    cells.append({"name": "collisionquad", "origin": "0 -100 0", "size": "200 100", "collisionbehavior": behavior,
                  "bouncefactor": 0.7})
w = Wallpaper('particles-collisionshapes', 'particles falling on a sphere and a quad', clear='0 0 0')
for k, operator in enumerate(cells):
    w.files['particles/c%d.json' % k] = cell(operator)
    w.add({"name": "%s %d" % (operator["name"], k), "particle": "particles/c%d.json" % k,
           "origin": "%d %d 0" % (400 + 560 * (k % 3), 700 - 400 * (k // 3)), "scale": "1 1 1", "angles": "0 0 0",
           "visible": True})
w.save()
clock_probe.stamp(os.path.join(HERE, 'particles-collisionshapes'))

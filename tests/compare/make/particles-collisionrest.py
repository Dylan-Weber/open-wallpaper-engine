# particles-collisionrest: single particles that come to rest on a collision shape, so the picture is exact (no
# stream to jitter): on a plane at y -100 (bounce 0.5 and slide), on the plane x = 100 (a wall, bounce 0.5), on a
# quad (0 -100, 200 x 100) at x 50, on a sphere (0 -100, radius 100) dropped onto its top, and sliding along a
# plane at 100 a second.
def cell(operator, origin, velocity, gravity="0 -500 0"):
    return {
        "material": "materials/particle/halo.json", "maxcount": 4, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"}],
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": origin, "rate": 0, "instantaneous": 1,
                     "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 100000, "max": 100000},
            {"id": 3, "name": "sizerandom", "min": 30, "max": 30},
            {"id": 4, "name": "velocityrandom", "min": velocity, "max": velocity},
        ],
        "operator": [{"id": 5, "name": "movement", "gravity": gravity, "drag": 0}, dict(operator, id=6)],
        "renderer": [{"id": 90, "name": "sprite"}],
    }


cells = (
    ({"name": "collisionplane", "distance": -100, "bouncefactor": 0.5}, "0 100 0", "0 0 0", "0 -500 0"),
    ({"name": "collisionplane", "distance": -100, "bouncefactor": 0.5, "collisionbehavior": "slide"}, "0 100 0",
     "100 0 0", "0 -500 0"),
    ({"name": "collisionplane", "plane": "1 0 0", "distance": -100, "bouncefactor": 0.5}, "100 0 0", "-200 0 0",
     "0 0 0"),
    ({"name": "collisionquad", "origin": "0 -100 0", "size": "200 100", "bouncefactor": 0.5}, "50 100 0", "0 0 0",
     "0 -500 0"),
    ({"name": "collisionsphere", "origin": "0 -100 0", "radius": 100, "bouncefactor": 0.5}, "0 200 0", "0 0 0",
     "0 -500 0"),
    ({"name": "collisionplane", "distance": -100, "bouncefactor": 1}, "0 100 0", "100 0 0", "0 -500 0"),
)
w = Wallpaper('particles-collisionrest', 'particles resting on collision shapes', clear='0 0 0')
for k, (operator, origin, velocity, gravity) in enumerate(cells):
    w.files['particles/c%d.json' % k] = cell(operator, origin, velocity, gravity)
    w.add({"name": "rest %d" % k, "particle": "particles/c%d.json" % k,
           "origin": "%d %d 0" % (400 + 560 * (k % 3), 700 - 400 * (k // 3)), "scale": "1 1 1", "angles": "0 0 0",
           "visible": True})
w.save()
clock_probe.stamp(os.path.join(HERE, 'particles-collisionrest'))

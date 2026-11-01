# particles-collisionquad-sides: collisionquad / collisionsphere from the other side and tilted. Cell 0: a quad (200 100
# at 0 -100) hit from below by a stream thrown up at 600; cell 1: the quad with angles 0 0 30, a stream falling on it;
# cell 2: the same with angles 30 0 0; cell 3: a sphere (radius 100 at 0 -100) with the stream born at its centre
# moving right at 100; cell 4: the quad with bouncefactor 0 (bounce); cell 5: the quad behaviour "bounce" explicitly
# with size 400 100.
def cell(operator, origin, velocity):
    return {
        "material": "materials/particle/halo.json", "maxcount": 200, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"}],
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": origin, "rate": 20, "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 4, "max": 4},
            {"id": 3, "name": "sizerandom", "min": 24, "max": 24},
            {"id": 4, "name": "velocityrandom", "min": velocity, "max": velocity},
        ],
        "operator": [{"id": 5, "name": "movement", "gravity": "0 -500 0", "drag": 0}, dict(operator, id=6)],
        "renderer": [{"id": 90, "name": "sprite"}],
    }


quad = {"name": "collisionquad", "origin": "0 -100 0", "size": "200 100", "bouncefactor": 0.7,
        "collisionbehavior": "bounce"}
cells = (
    (quad, "0 -250 0", "0 600 0"),
    (dict(quad, angles="0 0 30"), "-60 100 0", "40 200 0"),
    (dict(quad, angles="30 0 0"), "-60 100 0", "40 200 0"),
    ({"name": "collisionsphere", "origin": "0 -100 0", "radius": 100, "bouncefactor": 0.7,
      "collisionbehavior": "bounce"}, "0 -100 0", "100 0 0"),
    (dict(quad, bouncefactor=0), "-60 100 0", "40 200 0"),
    (dict(quad, size="400 100"), "-160 100 0", "40 200 0"),
)
w = Wallpaper('particles-collisionquad-sides', 'collision quad from below, tilted, sphere from inside', clear='0 0 0')
for k, (operator, origin, velocity) in enumerate(cells):
    w.files['particles/c%d.json' % k] = cell(operator, origin, velocity)
    w.add({"name": "cell %d" % k, "particle": "particles/c%d.json" % k,
           "origin": "%d %d 0" % (400 + 560 * (k % 3), 700 - 400 * (k // 3)), "scale": "1 1 1", "angles": "0 0 0",
           "visible": True})
w.save()
clock_probe.stamp(os.path.join(HERE, 'particles-collisionquad-sides'))

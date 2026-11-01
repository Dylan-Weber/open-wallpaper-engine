# particles-collisionbounds: the collisionbounds operator with the screen as its bounds. Streams thrown right at 300 and
# up at 300 under gravity -500 from near the right edge (1500) and, in the lower cells, near the floor: default
# behaviour, bouncefactor 0.5, behaviour slide, behaviour delete; then a stream at the left edge and one at the top.
def cell(operator, velocity="300 300 0"):
    return {
        "material": "materials/particle/halo.json", "maxcount": 200, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"}],
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "0 0 0", "rate": 20, "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 4, "max": 4},
            {"id": 3, "name": "sizerandom", "min": 24, "max": 24},
            {"id": 4, "name": "velocityrandom", "min": velocity, "max": velocity},
        ],
        "operator": [{"id": 5, "name": "movement", "gravity": "0 -500 0", "drag": 0}, dict(operator, id=6)],
        "renderer": [{"id": 90, "name": "sprite"}],
    }


bounds = {"name": "collisionbounds"}
cells = (
    (bounds, "1500 200", "300 300 0"),
    (dict(bounds, bouncefactor=0.5), "1000 200", "300 300 0"),
    (dict(bounds, collisionbehavior="slide"), "1500 600", "300 300 0"),
    (dict(bounds, collisionbehavior="delete"), "1000 600", "300 300 0"),
    (bounds, "300 200", "-300 300 0"),
    (bounds, "300 900", "100 600 0"),
)
w = Wallpaper('particles-collisionbounds', 'particles bouncing off the screen edges', clear='0 0 0')
for k, (operator, origin, velocity) in enumerate(cells):
    w.files['particles/c%d.json' % k] = cell(operator, velocity)
    w.add({"name": "bounds %d" % k, "particle": "particles/c%d.json" % k, "origin": origin + " 0",
           "scale": "1 1 1", "angles": "0 0 0", "visible": True})
w.save()
clock_probe.stamp(os.path.join(HERE, 'particles-collisionbounds'))

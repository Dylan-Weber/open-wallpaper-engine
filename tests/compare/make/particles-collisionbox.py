# particles-collisionbox: the collisionbox operator, which owe doesn't know. Single particles with gravity, dropped (or thrown)
# at a box, to see where they come to rest: the keys (origin, size as 2 or 3 numbers, angles), the faces and the defaults.
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


box = {"name": "collisionbox", "bouncefactor": 0}
big = dict(box, origin="0 -100 0", size="200 100 100")
cells = (
    (big, "0 -100 0", "300 0 0", "0 0 0"),
    (big, "0 -100 0", "-300 0 0", "0 0 0"),
    (big, "0 -100 0", "0 300 0", "0 0 0"),
    (big, "0 -100 0", "0 -300 0", "0 0 0"),
    (dict(box, origin="0 -100 0", size="200 100"), "0 -100 0", "300 0 0", "0 0 0"),
    (dict(box, origin="0 -100 0", size="200 100"), "0 -100 0", "0 300 0", "0 0 0"),
    (big, "0 -100 0", "300 300 0", "0 0 0"),
    (dict(box), "0 0 0", "300 0 0", "0 0 0"),
    (dict(box), "0 0 0", "0 300 0", "0 0 0"),
    (dict(big, bouncefactor=1), "0 -100 0", "300 0 0", "0 0 0"),
    (dict(big, collisionbehavior="delete"), "0 -100 0", "300 0 0", "0 0 0"),
    (dict(big, collisionbehavior="slide"), "0 -100 0", "300 100 0", "0 0 0"),
)
w = Wallpaper('particles-collisionbox', 'particles meeting a collision box', clear='0 0 0')
for k, (operator, origin, velocity, gravity) in enumerate(cells):
    w.files['particles/c%d.json' % k] = cell(operator, origin, velocity, gravity)
    w.add({"name": "box %d" % k, "particle": "particles/c%d.json" % k,
           "origin": "%d %d 0" % (340 + 440 * (k % 4), 850 - 330 * (k // 4)), "scale": "1 1 1", "angles": "0 0 0",
           "visible": True})
w.save()
clock_probe.stamp(os.path.join(HERE, 'particles-collisionbox'))

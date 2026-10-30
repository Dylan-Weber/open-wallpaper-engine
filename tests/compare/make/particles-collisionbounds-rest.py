# particles-collisionbounds-rest: single particles that come to rest against the screen's edges under collisionbounds
# (bouncefactor 0, so they stay where they meet it): the right wall, the left wall and floor (a corner), the floor, the
# ceiling; then the right wall and floor with behaviour slide and the floor with behaviour delete.
def cell(operator, origin, velocity, gravity):
    return {
        "material": "materials/particle/halo.json", "maxcount": 4, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"}],
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "0 0 0", "rate": 0, "instantaneous": 1,
                     "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 100000, "max": 100000},
            {"id": 3, "name": "sizerandom", "min": 30, "max": 30},
            {"id": 4, "name": "velocityrandom", "min": velocity, "max": velocity},
        ],
        "operator": [{"id": 5, "name": "movement", "gravity": gravity, "drag": 0}, dict(operator, id=6)],
        "renderer": [{"id": 90, "name": "sprite"}],
    }


bounds = {"name": "collisionbounds", "bouncefactor": 0}
cells = (
    (bounds, "1700 700", "300 0 0", "0 0 0"),
    (bounds, "200 700", "-300 -100 0", "0 0 0"),
    (bounds, "960 300", "0 0 0", "0 -500 0"),
    (bounds, "960 800", "0 400 0", "0 0 0"),
    (dict(bounds, collisionbehavior="slide"), "1700 300", "300 0 0", "0 -500 0"),
    (dict(bounds, collisionbehavior="delete"), "1300 300", "0 0 0", "0 -500 0"),
)
w = Wallpaper('particles-collisionbounds-rest', 'particles resting against the screen edges', clear='0 0 0')
for k, (operator, origin, velocity, gravity) in enumerate(cells):
    w.files['particles/c%d.json' % k] = cell(operator, origin, velocity, gravity)
    w.add({"name": "bounds %d" % k, "particle": "particles/c%d.json" % k, "origin": origin + " 0",
           "scale": "1 1 1", "angles": "0 0 0", "visible": True})
w.save()
clock_probe.stamp(os.path.join(HERE, 'particles-collisionbounds-rest'))

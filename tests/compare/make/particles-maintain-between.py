# particles-maintain-between: maintaindistancebetweencontrolpoints with control point 0 at the object and control
# point 1 200 to its right. Single particles that live for good, each started at some place with some velocity
# (movement, no gravity); where they end up at the capture shows what the operator does.
def cell(position, velocity, extra=None):
    operator = {"id": 6, "name": "maintaindistancebetweencontrolpoints"}
    operator.update(extra or {})
    return {
        "material": "materials/particle/halo.json", "maxcount": 4, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"}, {"id": 1, "flags": 0, "offset": "200 0 0"}],
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": position, "rate": 0, "instantaneous": 1,
                     "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 100000, "max": 100000},
            {"id": 3, "name": "sizerandom", "min": 30, "max": 30},
            {"id": 4, "name": "velocityrandom", "min": velocity, "max": velocity},
        ],
        "operator": [{"id": 5, "name": "movement"}, operator],
        "renderer": [{"id": 90, "name": "sprite"}],
    }


cells = (
    ("20 100 0", "0 0 0"), ("60 100 0", "0 0 0"), ("100 100 0", "0 0 0"), ("140 100 0", "0 0 0"),
    ("180 100 0", "0 0 0"), ("100 0 0", "0 0 0"), ("100 -100 0", "0 0 0"), ("-50 100 0", "0 0 0"),
    ("250 100 0", "0 0 0"), ("100 100 0", "30 0 0"), ("400 0 0", "-30 0 0"), ("100 100 0", "0 30 0"),
)
w = Wallpaper('particles-maintain-between', 'particles kept at a distance between control points', clear='0 0 0')
for k, (position, velocity) in enumerate(cells):
    w.files['particles/c%d.json' % k] = cell(position, velocity)
    w.add({"name": "between %d" % k, "particle": "particles/c%d.json" % k,
           "origin": "%d %d 0" % (200 + 430 * (k % 4), 850 - 330 * (k // 4)), "scale": "1 1 1", "angles": "0 0 0",
           "visible": True})
w.save()
clock_probe.stamp(os.path.join(HERE, 'particles-maintain-between'))

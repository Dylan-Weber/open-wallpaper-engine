# particles-inherit-event: inheritinitialvaluefromevent and inheritvaluefromevent. A parent particle (orange 255 128 0,
# size 100, opacity 0.5, velocity 30 to the right, living for good) has a child system, made when it is born (eventspawn) or following it
# (eventfollow), whose particle is white, size 30, still. Cells: eventspawn child alone, with inheritinitialvaluefromevent;
# eventfollow child alone, with inheritvaluefromevent (an operator).
def parent(child_type):
    return {
        "material": "materials/particle/halo.json", "maxcount": 4, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"}],
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "0 0 0", "rate": 0, "instantaneous": 1,
                     "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 100000, "max": 100000},
            {"id": 3, "name": "sizerandom", "min": 100, "max": 100},
            {"id": 4, "name": "colorrandom", "min": "255 128 0", "max": "255 128 0"},
            {"id": 5, "name": "velocityrandom", "min": "30 0 0", "max": "30 0 0"},
            {"id": 7, "name": "alpharandom", "min": 0.5, "max": 0.5},
        ],
        "operator": [{"id": 6, "name": "movement"}],
        "renderer": [{"id": 90, "name": "sprite"}],
        "children": [{"id": 15, "name": "particles/child.json", "type": child_type, "maxcount": 4, "controlpointid": 0,
                      "probability": 1, "origin": "0 0 0", "scale": "1 1 1", "angles": "0 0 0"}],
    }


def child(initializer=(), operators=()):
    return {
        "material": "materials/particle/halo.json", "maxcount": 4, "starttime": 0, "flags": 0,
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "0 0 0", "rate": 0, "instantaneous": 1,
                     "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 100000, "max": 100000},
            {"id": 3, "name": "sizerandom", "min": 30, "max": 30},
            {"id": 4, "name": "colorrandom", "min": "255 255 255", "max": "255 255 255"},
        ] + list(initializer),
        "operator": [{"id": 6, "name": "movement"}] + list(operators),
        "renderer": [{"id": 90, "name": "sprite"}],
    }


cells = (
    ("eventspawn alone", "eventspawn", [], []),
    ("eventspawn inheritinitialvaluefromevent", "eventspawn", [{"id": 9, "name": "inheritinitialvaluefromevent"}], []),
    ("eventfollow alone", "eventfollow", [], []),
    ("eventfollow inheritvaluefromevent", "eventfollow", [], [{"id": 9, "name": "inheritvaluefromevent"}]),
)
w = Wallpaper('particles-inherit-event', 'child particles inheriting from the parent', clear='0 0 0')
for k, (name, kind, initializer, operators) in enumerate(cells):
    w.files['particles/p%d.json' % k] = dict(parent(kind), children=[
        dict(parent(kind)["children"][0], name="particles/child%d.json" % k)])
    w.files['particles/child%d.json' % k] = child(initializer, operators)
    w.add({"name": name, "particle": "particles/p%d.json" % k,
           "origin": "%d %d 0" % (200 + 400 * (k % 4), 700), "scale": "1 1 1", "angles": "0 0 0", "visible": True})
w.save()
clock_probe.stamp(os.path.join(HERE, 'particles-inherit-event'))

# particles-event-children: where a child particle system's particle is under eventspawn, eventfollow and eventdeath. The
# parent particle is orange, size 100, moves right at 30 a second and lives for good (3 s for eventdeath); the child
# particle is white, size 30, moves right at 50 a second, in the child's own space (flags 0) or the world's (flags 1).
def parent(child_type, child_name, lifetime):
    return {
        "material": "materials/particle/halo.json", "maxcount": 4, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"}],
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "0 0 0", "rate": 0, "instantaneous": 1,
                     "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": lifetime, "max": lifetime},
            {"id": 3, "name": "sizerandom", "min": 100, "max": 100},
            {"id": 4, "name": "colorrandom", "min": "255 128 0", "max": "255 128 0"},
            {"id": 5, "name": "velocityrandom", "min": "30 0 0", "max": "30 0 0"},
        ],
        "operator": [{"id": 6, "name": "movement"}],
        "renderer": [{"id": 90, "name": "sprite"}],
        "children": [{"id": 15, "name": child_name, "type": child_type, "maxcount": 4, "controlpointid": 0,
                      "probability": 1, "origin": "0 0 0", "scale": "1 1 1", "angles": "0 0 0"}],
    }


def child(flags):
    return {
        "material": "materials/particle/halo.json", "maxcount": 4, "starttime": 0, "flags": flags,
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "0 0 0", "rate": 0, "instantaneous": 1,
                     "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 100000, "max": 100000},
            {"id": 3, "name": "sizerandom", "min": 30, "max": 30},
            {"id": 4, "name": "colorrandom", "min": "255 255 255", "max": "255 255 255"},
            {"id": 5, "name": "velocityrandom", "min": "50 0 0", "max": "50 0 0"},
        ],
        "operator": [{"id": 6, "name": "movement"}],
        "renderer": [{"id": 90, "name": "sprite"}],
    }


cells = []
for kind, lifetime in (("eventspawn", 100000), ("eventfollow", 100000), ("eventdeath", 3)):
    for flags in (0, 1):
        cells.append((kind, lifetime, flags))
w = Wallpaper('particles-event-children', 'child particles under the three event types', clear='0 0 0')
for k, (kind, lifetime, flags) in enumerate(cells):
    w.files['particles/p%d.json' % k] = parent(kind, 'particles/child%d.json' % k, lifetime)
    w.files['particles/child%d.json' % k] = child(flags)
    w.add({"name": "%s flags %d" % (kind, flags), "particle": "particles/p%d.json" % k,
           "origin": "%d %d 0" % (200 + 600 * (k % 3), 850 - 300 * (k // 3)), "scale": "1 1 1", "angles": "0 0 0",
           "visible": True})
w.save()
clock_probe.stamp(os.path.join(HERE, 'particles-event-children'))

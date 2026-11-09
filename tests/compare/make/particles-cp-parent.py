# particles-cp-parent: a static child's control points taken from its parent's ("parentcontrolpoint", control point flag 4),
# as dripping_water's splash/refract children (CP0 <- parent CP1, CP1 <- parent CP2) and droplets (flag 4) use them. The
# parent draws nothing; its control points 1 and 2 sit at 300 0 and 0 -250 from it. Each child draws two particles at rest,
# at its control points 0 and 1 (mapsequencebetweencontrolpoints, count 2). Children: no mapping (control: both at the
# child's own offsets 0 0 and 120 120); parentcontrolpoint 1 and 2; the same with flag 4; flag 4 with offsets 40 0 / 0 40
# on top; parentcontrolpoint without flag 4 and with offsets.
def child(cps):
    return {
        "material": "materials/particle/halo.json", "maxcount": 2, "starttime": 0, "flags": 0,
        "controlpoint": cps,
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "0 0 0", "rate": 0, "instantaneous": 2,
                     "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 100000, "max": 100000},
            {"id": 3, "name": "sizerandom", "min": 30, "max": 30},
            {"id": 4, "name": "mapsequencebetweencontrolpoints", "count": 2},
        ],
        "operator": [{"id": 5, "name": "movement", "drag": 0}],
        "renderer": [{"id": 90, "name": "sprite"}],
    }


def parent(child_name):
    return {
        "material": "materials/particle/halo.json", "maxcount": 1, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "offset": "0 0 0"}, {"id": 1, "offset": "300 0 0"}, {"id": 2, "offset": "0 -250 0"}],
        "emitter": [], "initializer": [], "operator": [], "renderer": [],
        "children": [{"id": 9, "maxcount": 1, "name": "particles/%s.json" % child_name}],
    }


children = (
    ("own", [{"id": 0, "offset": "0 0 0"}, {"id": 1, "offset": "120 120 0"}]),
    ("parent 1 2", [{"id": 0, "parentcontrolpoint": 1}, {"id": 1, "parentcontrolpoint": 2}]),
    ("flag 4", [{"id": 0, "flags": 4, "parentcontrolpoint": 1}, {"id": 1, "flags": 4, "parentcontrolpoint": 2}]),
    ("flag 4 offsets", [{"id": 0, "flags": 4, "parentcontrolpoint": 1, "offset": "40 0 0"},
                        {"id": 1, "flags": 4, "parentcontrolpoint": 2, "offset": "0 40 0"}]),
    ("parent offsets", [{"id": 0, "parentcontrolpoint": 1, "offset": "40 0 0"},
                        {"id": 1, "parentcontrolpoint": 2, "offset": "0 40 0"}]),
)
name = 'particles-cp-parent'
w = Wallpaper(name, 'child control points from the parent: parentcontrolpoint and flag 4', clear='0 0 0')
for k, (label, cps) in enumerate(children):
    w.files['particles/child%d.json' % k] = child(cps)
    w.files['particles/parent%d.json' % k] = parent('child%d' % k)
    w.add({"name": label, "particle": "particles/parent%d.json" % k,
           "origin": "%d %d 0" % (200 + 560 * (k % 3), 850 - 450 * (k // 3)), "scale": "1 1 1", "angles": "0 0 0"})
w.save()
clock_probe.stamp(os.path.join(HERE, name))
write(name + '/compare.json', {"probe": True, "fps": 30})

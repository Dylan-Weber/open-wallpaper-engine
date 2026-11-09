# particles-sequence-arc: mapsequencebetweencontrolpoints' arcamount and flags, as thunderbolt (arcamount 0.1, flags 23;
# its beam child flags 19, count 8) uses them. Eight particles at rest spaced from control point 0 (-200 0) to 1 (200 0).
# Cells: arcamount 0, 0.1, 0.5; arcamount 0.5 with flags 1, 2, 4, 16 alone; arcamount 0.1 with flags 23, 19 and 7.
def cell(sequence):
    return {
        "material": "materials/particle/halo.json", "maxcount": 8, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "-200 0 0"}, {"id": 1, "flags": 0, "offset": "200 0 0"}],
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "0 0 0", "rate": 0, "instantaneous": 8,
                     "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 100000, "max": 100000},
            {"id": 3, "name": "sizerandom", "min": 16, "max": 16},
            dict({"id": 4, "name": "mapsequencebetweencontrolpoints", "count": 8}, **sequence),
        ],
        "operator": [{"id": 5, "name": "movement", "drag": 0}],
        "renderer": [{"id": 90, "name": "sprite"}],
    }


cells = (
    ("arc 0", {"arcamount": 0}),
    ("arc 0.1", {"arcamount": 0.1}),
    ("arc 0.5", {"arcamount": 0.5}),
    ("arc 0.5 flags 1", {"arcamount": 0.5, "flags": 1}),
    ("arc 0.5 flags 2", {"arcamount": 0.5, "flags": 2}),
    ("arc 0.5 flags 4", {"arcamount": 0.5, "flags": 4}),
    ("arc 0.5 flags 16", {"arcamount": 0.5, "flags": 16}),
    ("arc 0.1 flags 23", {"arcamount": 0.1, "flags": 23}),
    ("arc 0.1 flags 19", {"arcamount": 0.1, "flags": 19}),
    ("arc 0.1 flags 7", {"arcamount": 0.1, "flags": 7}),
)
name = 'particles-sequence-arc'
w = Wallpaper(name, 'mapsequencebetweencontrolpoints: arcamount and flags', clear='0 0 0')
for k, (label, sequence) in enumerate(cells):
    w.files['particles/c%d.json' % k] = cell(sequence)
    w.add({"name": label, "particle": "particles/c%d.json" % k,
           "origin": "%d %d 0" % (330 + 630 * (k % 3), 900 - 220 * (k // 3)), "scale": "1 1 1", "angles": "0 0 0"})
w.save()
clock_probe.stamp(os.path.join(HERE, name))
write(name + '/compare.json', {"probe": True, "fps": 30, "shots": 3, "shot_interval_ms": 500})

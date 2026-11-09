# particles-turbulence-stats: how hard and how fast-changing the turbulence operator is (owe's curls back where Wallpaper
# Engine's drifts). Each row is a stream (5 a second, moving right at 200 units/s, life 4 s) under turbulence, so the
# spread of the dots at each age measures it statistically: speed 100 / 500 at scale 0.01; scale 0.005 / 0.02 at speed
# 500; timescale 0 and 5; mask 0 1 0 (vertical only).
def cell(turbulence):
    return {
        "material": "materials/particle/halo.json", "maxcount": 40, "starttime": 0, "flags": 0,
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "0 0 0", "rate": 5, "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 4, "max": 4},
            {"id": 3, "name": "sizerandom", "min": 12, "max": 12},
            {"id": 4, "name": "velocityrandom", "min": "200 0 0", "max": "200 0 0"},
        ],
        "operator": [{"id": 5, "name": "movement", "drag": 0, "gravity": "0 0 0"},
                     dict({"id": 6, "name": "turbulence"}, **turbulence)],
        "renderer": [{"id": 90, "name": "sprite"}],
    }


base = {"speedmin": 500, "speedmax": 500, "scale": 0.01, "timescale": 1}
rows = (
    ("speed 100", dict(base, speedmin=100, speedmax=100)),
    ("speed 500", dict(base)),
    ("scale 0.005", dict(base, scale=0.005)),
    ("scale 0.02", dict(base, scale=0.02)),
    ("timescale 0", dict(base, timescale=0)),
    ("timescale 5", dict(base, timescale=5)),
    ("mask 0 1 0", dict(base, mask="0 1 0")),
)
name = 'particles-turbulence-stats'
w = Wallpaper(name, 'turbulence: strength and spatial scale, statistically', clear='0 0 0')
for k, (label, turbulence) in enumerate(rows):
    w.files['particles/c%d.json' % k] = cell(turbulence)
    w.add({"name": label, "particle": "particles/c%d.json" % k,
           "origin": "%d %d 0" % (250, 990 - 115 * k), "scale": "1 1 1", "angles": "0 0 0"})
w.save()
clock_probe.stamp(os.path.join(HERE, name))
write(name + '/compare.json', {"probe": True, "fps": 30, "shots": 30, "shot_interval_ms": 150})

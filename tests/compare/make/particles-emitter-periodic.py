# particles-emitter-periodic: emitter timing (thunderbolt's flicker). Every particle moves right at 200 units/s for 3 s,
# so each row shows the last 3 s of its emitter's history (newest on the left, a dot every 0.1 s at rate 10) and its gaps.
# Rows, top to bottom: plain; flags 4 with periodic delay 1 and duration 1; flags 4, delay 0.5, duration 1; flags 4,
# delay 1, duration 1, at most 5 a period; the periodic keys without flags 4; a start delay of 2 s; a duration of 2 s;
# thunderbolt's emitter (rate 100, at most 32 a period, flags 4, delay 1, duration 1).
def cell(emitter):
    return {
        "material": "materials/particle/halo.json", "maxcount": 400, "starttime": 0, "flags": 0,
        "emitter": [dict({"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                          "directions": "1 1 0", "origin": "0 0 0", "rate": 10, "speedmin": 0, "speedmax": 0},
                         **emitter)],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 3, "max": 3},
            {"id": 3, "name": "sizerandom", "min": 12, "max": 12},
            {"id": 4, "name": "velocityrandom", "min": "200 0 0", "max": "200 0 0"},
        ],
        "operator": [{"id": 5, "name": "movement", "drag": 0, "gravity": "0 0 0"}],
        "renderer": [{"id": 90, "name": "sprite"}],
    }


periodic = {"flags": 4, "minperiodicdelay": 1, "maxperiodicdelay": 1, "minperiodicduration": 1, "maxperiodicduration": 1}
rows = (
    ("plain", {}),
    ("periodic 1/1", dict(periodic)),
    ("periodic delay 0.5", dict(periodic, minperiodicdelay=0.5, maxperiodicdelay=0.5)),
    ("periodic, 5 a period", dict(periodic, maxtoemitperperiod=5)),
    ("periodic keys, no flag", dict(periodic, flags=0)),
    ("delay 2", {"delay": 2}),
    ("duration 2", {"duration": 2}),
    ("thunderbolt", dict(periodic, rate=100, maxtoemitperperiod=32)),
)
name = 'particles-emitter-periodic'
w = Wallpaper(name, 'emitter timing: periodic emission, delay, duration', clear='0 0 0')
for k, (label, emitter) in enumerate(rows):
    w.files['particles/c%d.json' % k] = cell(emitter)
    w.add({"name": label, "particle": "particles/c%d.json" % k,
           "origin": "%d %d 0" % (500, 1000 - 95 * k), "scale": "1 1 1", "angles": "0 0 0"})
w.save()
clock_probe.stamp(os.path.join(HERE, name))
write(name + '/compare.json', {"probe": True, "fps": 30, "shots": 20, "shot_interval_ms": 150})

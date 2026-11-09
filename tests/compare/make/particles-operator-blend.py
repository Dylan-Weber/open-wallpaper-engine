# particles-operator-blend: operators' blend windows (blendinstart..blendinend, blendoutstart..blendoutend: fractions of a
# particle's life), as thunderbolt's turbulence and oscillateposition use them. Each row is a stream (5 a second, moving
# right at 200 units/s, life 4 s), so a particle's place along the row is its age. Rows: plain; capvelocity 50; capvelocity
# 50 blended in over 0.25..0.5; capvelocity 50 blended out over 0.25..0.5; turbulence; turbulence blended in over
# 0.5..0.75; oscillateposition; oscillateposition blended in over 0.5..0.75.
def cell(op):
    operators = [{"id": 5, "name": "movement", "drag": 0, "gravity": "0 0 0"}]
    if op:
        operators.append(dict({"id": 6}, **op))
    return {
        "material": "materials/particle/halo.json", "maxcount": 40, "starttime": 0, "flags": 0,
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "0 0 0", "rate": 5, "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 4, "max": 4},
            {"id": 3, "name": "sizerandom", "min": 14, "max": 14},
            {"id": 4, "name": "velocityrandom", "min": "200 0 0", "max": "200 0 0"},
        ],
        "operator": operators,
        "renderer": [{"id": 90, "name": "sprite"}],
    }


cap = {"name": "capvelocity", "maxspeed": 50}
turbulence = {"name": "turbulence", "speedmin": 500, "speedmax": 500, "timescale": 1, "scale": 0.01}
oscillate = {"name": "oscillateposition", "frequencymin": 1, "frequencymax": 1, "scalemin": 30, "scalemax": 30,
             "mask": "0 1 0"}
rows = (
    ("plain", None),
    ("capvelocity", dict(cap)),
    ("capvelocity in 0.25..0.5", dict(cap, blendinstart=0.25, blendinend=0.5)),
    ("capvelocity out 0.25..0.5", dict(cap, blendoutstart=0.25, blendoutend=0.5)),
    ("turbulence", dict(turbulence)),
    ("turbulence in 0.5..0.75", dict(turbulence, blendinstart=0.5, blendinend=0.75)),
    ("oscillateposition", dict(oscillate)),
    ("oscillateposition in 0.5..0.75", dict(oscillate, blendinstart=0.5, blendinend=0.75)),
)
name = 'particles-operator-blend'
w = Wallpaper(name, 'operator blend windows: capvelocity, turbulence, oscillateposition', clear='0 0 0')
for k, (label, op) in enumerate(rows):
    w.files['particles/c%d.json' % k] = cell(op)
    w.add({"name": label, "particle": "particles/c%d.json" % k,
           "origin": "%d %d 0" % (250, 1000 - 95 * k), "scale": "1 1 1", "angles": "0 0 0"})
w.save()
clock_probe.stamp(os.path.join(HERE, name))
write(name + '/compare.json', {"probe": True, "fps": 30, "shots": 20, "shot_interval_ms": 150})

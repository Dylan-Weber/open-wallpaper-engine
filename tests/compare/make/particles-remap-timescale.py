# particles-remap-timescale: remapvalue from particlesystemtime to opacity (thunderbolt's flicker) with
# transforminputscale and a blend window. One particle at rest per cell, its brightness read over a series of shots:
# calibration (alpha 1 and 0.5), sine with scale 1, 6 and 0.5, a linear ramp with scale 0.1, sine with scale 6 over
# inputrangemax 2, and sine with scale 6 blended in over 0.5..0.6 of a long life (at the shots' age: none) and over
# 0.3..0.5 of a 30 s life (blending in while the shots are taken), and a remap to 0 blended out over 0.3..0.5.
def cell(remap=None, lifetime=100000, alpha=1.0):
    operators = [{"id": 5, "name": "movement", "drag": 0}]
    if remap is not None:
        operators.append(dict({"id": 6, "name": "remapvalue", "input": "particlesystemtime", "output": "opacity"},
                              **remap))
    return {
        "material": "materials/particle/halo.json", "maxcount": 1, "starttime": 0, "flags": 0,
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "0 0 0", "rate": 0, "instantaneous": 1,
                     "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": lifetime, "max": lifetime},
            {"id": 3, "name": "sizerandom", "min": 80, "max": 80},
            {"id": 4, "name": "alpharandom", "min": alpha, "max": alpha},
        ],
        "operator": operators,
        "renderer": [{"id": 90, "name": "sprite"}],
    }


sine = {"transformfunction": "sine"}
cells = (
    ("alpha 1", None, 100000, 1.0),
    ("alpha 0.5", None, 100000, 0.5),
    ("sine", dict(sine), 100000, 1.0),
    ("sine, scale 6", dict(sine, transforminputscale=6), 100000, 1.0),
    ("sine, scale 0.5", dict(sine, transforminputscale=0.5), 100000, 1.0),
    ("linear, scale 0.1", {"transforminputscale": 0.1}, 100000, 1.0),
    ("sine, scale 6, range 2", dict(sine, transforminputscale=6, inputrangemax=2), 100000, 1.0),
    ("sine, scale 6, blend 0.5..0.6", dict(sine, transforminputscale=6, blendinstart=0.5, blendinend=0.6), 100000, 1.0),
    ("sine, scale 6, blend 0.3..0.5 of 30 s", dict(sine, transforminputscale=6, blendinstart=0.3, blendinend=0.5), 30, 1.0),
    ("to 0, blend out 0.3..0.5 of 30 s", {"outputrangemax": 0, "blendoutstart": 0.3, "blendoutend": 0.5}, 30, 1.0),
)
# second wallpaper: the input inside its range (inputrangemax 40: 0.3 to 0.6 while the shots are taken), so the
# transforms' shapes show: linear and sine at scales 1, 2, 0.5 and 6, triangle and square at scale 1
r40 = {"inputrangemax": 40}
cells_in_range = (
    ("alpha 1", None, 100000, 1.0),
    ("linear", dict(r40), 100000, 1.0),
    ("linear, scale 2", dict(r40, transforminputscale=2), 100000, 1.0),
    ("linear, scale 0.5", dict(r40, transforminputscale=0.5), 100000, 1.0),
    ("sine", dict(r40, **sine), 100000, 1.0),
    ("sine, scale 2", dict(r40, transforminputscale=2, **sine), 100000, 1.0),
    ("sine, scale 0.5", dict(r40, transforminputscale=0.5, **sine), 100000, 1.0),
    ("sine, scale 6", dict(r40, transforminputscale=6, **sine), 100000, 1.0),
    ("triangle", dict(r40, transformfunction="triangle"), 100000, 1.0),
    ("square", dict(r40, transformfunction="square"), 100000, 1.0),
)
for name, cells in (('particles-remap-timescale', cells), ('particles-remap-timescale-range', cells_in_range)):
    fps = 30
    w = Wallpaper(name, 'remapvalue from the system time: transforminputscale and blend windows', clear='0 0 0')
    for k, (label, remap, lifetime, alpha) in enumerate(cells):
        w.files['particles/c%d.json' % k] = cell(remap, lifetime, alpha)
        w.add({"name": label, "particle": "particles/c%d.json" % k,
               "origin": "%d %d 0" % (240 + 360 * (k % 5), 760 - 400 * (k // 5)), "scale": "1 1 1", "angles": "0 0 0"})
    w.save()
    clock_probe.stamp(os.path.join(HERE, name))
    write(name + '/compare.json', {"probe": True, "fps": fps, "shots": 60, "shot_interval_ms": 100})

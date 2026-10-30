# particles-reduce-near-rest: single particles thrown at 200 units/s at control point 1, 400 units out, slowed by
# reducemovementnearcontrolpoint (inner 50, outer 150, reductionouter 0) with reductioninner 3, 10 and 30, so they
# come to rest short of it; without the operator they fly off the screen.
def single(reduction):
    return {
        "material": "materials/particle/halo.json", "maxcount": 4, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"}, {"id": 1, "flags": 0, "offset": "400 0 0"}],
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "0 0 0", "rate": 0, "instantaneous": 1,
                     "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 100000, "max": 100000},
            {"id": 3, "name": "sizerandom", "min": 120, "max": 120},
            {"id": 4, "name": "colorrandom", "min": "255 220 80", "max": "255 220 80"},
            {"id": 5, "name": "velocityrandom", "min": "200 0 0", "max": "200 0 0"},
        ],
        "operator": [{"id": 20, "name": "movement", "drag": 0, "gravity": "0 0 0"},
                     {"id": 21, "name": "reducemovementnearcontrolpoint", "controlpoint": 1, "distanceinner": 50,
                      "distanceouter": 150, "reductioninner": reduction, "reductionouter": 0}],
        "renderer": [{"id": 90, "name": "sprite"}],
    }


w = Wallpaper('particles-reduce-near-rest', 'particles brought to rest near a control point', clear='0 0 0')
for k, reduction in enumerate((3, 10, 30)):
    w.files['particles/r%d.json' % k] = single(reduction)
    w.add({"name": "reduction %d" % reduction, "particle": "particles/r%d.json" % k,
           "origin": "300 %d 0" % (850 - 220 * k), "scale": "1 1 1", "angles": "0 0 0", "visible": True})
w.save()
clock_probe.stamp(os.path.join(HERE, 'particles-reduce-near-rest'))

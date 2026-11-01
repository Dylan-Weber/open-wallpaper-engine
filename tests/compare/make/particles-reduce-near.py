# particles-reduce-near: reducemovementnearcontrolpoint slows particles near a control point: streams emitted 4 a
# second at 200 units/s to the right, living 6 s, through control point 1 400 units out (inner distance 50, outer
# 150, reductionouter 0), with reductioninner 1, 3 and 10; and one without the operator.
def stream(operators):
    return {
        "material": "materials/particle/halo.json", "maxcount": 40, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"}, {"id": 1, "flags": 0, "offset": "400 0 0"}],
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "0 0 0", "rate": 4, "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 6, "max": 6},
            {"id": 3, "name": "sizerandom", "min": 30, "max": 30},
            {"id": 4, "name": "colorrandom", "min": "255 220 80", "max": "255 220 80"},
            {"id": 5, "name": "velocityrandom", "min": "200 0 0", "max": "200 0 0"},
        ],
        "operator": [{"id": 20, "name": "movement", "drag": 0, "gravity": "0 0 0"}] + operators,
        "renderer": [{"id": 90, "name": "sprite"}],
    }


w = Wallpaper('particles-reduce-near', 'particles slowed near a control point', clear='0 0 0')
rows = [('free', [])] + [('reduce%d' % r, [{"id": 21, "name": "reducemovementnearcontrolpoint", "controlpoint": 1,
                                            "distanceinner": 50, "distanceouter": 150, "reductioninner": r,
                                            "reductionouter": 0}]) for r in (1, 3, 10)]
for k, (name, operators) in enumerate(rows):
    w.files['particles/%s.json' % name] = stream(operators)
    w.add({"name": name, "particle": "particles/%s.json" % name, "origin": "150 %d 0" % (900 - 160 * k),
           "scale": "1 1 1", "angles": "0 0 0", "visible": True})
w.save()
with open(os.path.join(HERE, 'particles-reduce-near', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'particles-reduce-near'))

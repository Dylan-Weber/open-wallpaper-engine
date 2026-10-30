# particles-remap-velocity: remapvalue with a velocity output and a constant input (lifetime fraction over 0..0.00001 is
# always 1). Cells: set 100 a second, add 0.01, multiply by 0.999 on a start velocity of 200, subtract 0.01 from 10. One particle each, starting at -300; where it is at the capture shows what the operator did.
def cell(remap, velocity="0 0 0", drag=0):
    return {
        "material": "materials/particle/halo.json", "maxcount": 4, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"}],
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "-300 0 0", "rate": 0, "instantaneous": 1,
                     "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 100000, "max": 100000},
            {"id": 3, "name": "sizerandom", "min": 60, "max": 60},
            {"id": 4, "name": "velocityrandom", "min": velocity, "max": velocity},
        ],
        "operator": [{"id": 5, "name": "movement", "drag": drag},
                     dict({"id": 6, "name": "remapvalue", "input": "lifetimefraction", "inputrangemin": 0,
                           "inputrangemax": 0.00001, "output": "velocity"}, **remap)],
        "renderer": [{"id": 90, "name": "sprite"}],
    }


cells = (
    ({"operation": "remap", "outputrangemin": "0 0 0", "outputrangemax": "100 0 0"}, "0 0 0", 0),
    ({"operation": "add", "outputrangemin": "0 0 0", "outputrangemax": "0.01 0 0"}, "0 0 0", 0),
    ({"operation": "multiply", "outputrangemin": "0 0 0", "outputrangemax": "0.999 1 1"}, "200 0 0", 0),
    ({"operation": "subtract", "outputrangemin": "0 0 0", "outputrangemax": "0.01 0 0"}, "10 0 0", 0),
)
w = Wallpaper('particles-remap-velocity', 'remapvalue velocity output', clear='0 0 0')
for k, (remap, velocity, drag) in enumerate(cells):
    w.files['particles/c%d.json' % k] = cell(remap, velocity, drag)
    w.add({"name": "velocity %d" % k, "particle": "particles/c%d.json" % k,
           "origin": "960 %d 0" % (900 - 220 * k), "scale": "1 1 1", "angles": "0 0 0", "visible": True})
w.save()
clock_probe.stamp(os.path.join(HERE, 'particles-remap-velocity'))

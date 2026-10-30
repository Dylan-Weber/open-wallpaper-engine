# particles-remap-controlpoints: remapvalue's control point inputs. Control point 0 is at the object, control point 1 is
# 200 to its right; the particle (living for good, at rest) sits at 100 50 (or elsewhere for the last cells) and its colour
# is the input mapped over a range onto 0..1 per axis.
def cell(remap, position="100 50 0"):
    return {
        "material": "materials/particle/halo.json", "maxcount": 4, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"}, {"id": 1, "flags": 0, "offset": "200 0 0"}],
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": position, "rate": 0, "instantaneous": 1,
                     "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 100000, "max": 100000},
            {"id": 3, "name": "sizerandom", "min": 60, "max": 60},
        ],
        "operator": [dict({"id": 6, "name": "remapvalue", "output": "color", "operation": "remap",
                           "outputrangemin": "0 0 0", "outputrangemax": "1 1 1"}, **remap)],
        "renderer": [{"id": 90, "name": "sprite"}],
    }


cells = (
    ("delta to cp 1", {"input": "deltatocontrolpoint", "inputcontrolpoint0": 1, "inputrangemin": -200,
                       "inputrangemax": 200}, "100 50 0"),
    ("direction to cp 1", {"input": "directiontocontrolpoint", "inputcontrolpoint0": 1, "inputrangemin": -1,
                           "inputrangemax": 1}, "100 50 0"),
    ("between 0 and 1 at 100", {"input": "positionbetweentwocontrolpoints", "inputcontrolpoint0": 0,
                                "inputcontrolpoint1": 1, "inputrangemin": 0, "inputrangemax": 1}, "100 50 0"),
    ("between at 50", {"input": "positionbetweentwocontrolpoints", "inputcontrolpoint0": 0,
                       "inputcontrolpoint1": 1, "inputrangemin": 0, "inputrangemax": 1}, "50 80 0"),
    ("between at 250", {"input": "positionbetweentwocontrolpoints", "inputcontrolpoint0": 0,
                        "inputcontrolpoint1": 1, "inputrangemin": 0, "inputrangemax": 2}, "250 0 0"),
    ("delta to cp 0", {"input": "deltatocontrolpoint", "inputcontrolpoint0": 0, "inputrangemin": -200,
                       "inputrangemax": 200}, "100 50 0"),
)
w = Wallpaper('particles-remap-controlpoints', 'remapvalue control point inputs', clear='0 0 0')
for k, (name, remap, position) in enumerate(cells):
    w.files['particles/c%d.json' % k] = cell(remap, position)
    w.add({"name": name, "particle": "particles/c%d.json" % k,
           "origin": "%d %d 0" % (200 + 500 * (k % 3), 850 - 300 * (k // 3)), "scale": "1 1 1", "angles": "0 0 0",
           "visible": True})
w.save()
clock_probe.stamp(os.path.join(HERE, 'particles-remap-controlpoints'))

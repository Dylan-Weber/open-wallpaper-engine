# particles-remap-inputs: remapvalue's inputs. One particle per cell, at rest and living for good, whose colour is the input
# mapped over a range onto grey (0 to 1): the grey it ends up (at about 11.3 s) is the input's value over the range.
def cell(remap, lifetime=100000, extra_init=None):
    return {
        "material": "materials/particle/halo.json", "maxcount": 4, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"}, {"id": 1, "flags": 0, "offset": "100 0 0"}],
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "0 0 0", "rate": 0, "instantaneous": 1,
                     "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": lifetime, "max": lifetime},
            {"id": 3, "name": "sizerandom", "min": 60, "max": 60},
        ] + (extra_init or []),
        "operator": [dict({"id": 6, "name": "remapvalue", "output": "color", "operation": "remap",
                           "outputrangemin": "0 0 0", "outputrangemax": "1 1 1"}, **remap)],
        "renderer": [{"id": 90, "name": "sprite"}],
    }


cells = (
    ("lifetime", {"input": "lifetimefraction", "inputrangemin": 0, "inputrangemax": 1}, 20, None),
    ("system time", {"input": "particlesystemtime", "inputrangemin": 0, "inputrangemax": 20}, 100000, None),
    ("layer time", {"input": "layertime", "inputrangemin": 0, "inputrangemax": 20}, 100000, None),
    ("runtime", {"input": "runtime", "inputrangemin": 0, "inputrangemax": 20}, 100000, None),
    ("time of day", {"input": "timeofday", "inputrangemin": 0, "inputrangemax": 24}, 100000, None),
    ("velocity", {"input": "velocity", "inputrangemin": 0, "inputrangemax": 200}, 100000,
     [{"id": 4, "name": "velocityrandom", "min": "100 0 0", "max": "100 0 0"}]),
    ("opacity", {"input": "opacity", "inputrangemin": 0, "inputrangemax": 1}, 100000,
     [{"id": 4, "name": "alpharandom", "min": 0.3, "max": 0.3}]),
    ("rotation", {"input": "rotation", "inputrangemin": 0, "inputrangemax": 4}, 100000,
     [{"id": 4, "name": "rotationrandom", "min": "0 0 1", "max": "0 0 1"}]),
    ("angular speed", {"input": "angularspeed", "inputrangemin": 0, "inputrangemax": 4}, 100000,
     [{"id": 4, "name": "angularvelocityrandom", "min": "0 0 1", "max": "0 0 1"}]),
    ("layer origin", {"input": "layerorigin", "inputrangemin": 0, "inputrangemax": 2000}, 100000, None),
    ("distance to cp 1", {"input": "distancetocontrolpoint", "inputcontrolpoint0": 1, "inputrangemin": 0,
                          "inputrangemax": 200}, 100000, None),
    ("between cps", {"input": "positionbetweentwocontrolpoints", "inputcontrolpoint0": 0, "inputcontrolpoint1": 1,
                     "inputrangemin": 0, "inputrangemax": 1}, 100000, None),
)
w = Wallpaper('particles-remap-inputs', 'remapvalue inputs', clear='0 0 0')
for k, (name, remap, lifetime, extra) in enumerate(cells):
    w.files['particles/c%d.json' % k] = cell(remap, lifetime, extra)
    w.add({"name": name, "particle": "particles/c%d.json" % k,
           "origin": "%d %d 0" % (240 + 480 * (k % 4), 850 - 250 * (k // 4)), "scale": "1 1 1", "angles": "0 0 0",
           "visible": True})
w.save()
clock_probe.stamp(os.path.join(HERE, 'particles-remap-inputs'))

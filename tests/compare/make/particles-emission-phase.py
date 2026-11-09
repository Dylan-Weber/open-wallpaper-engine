# particles-emission-phase: streams at several rates, each particle thrown right at 200 units/s with nothing else
# acting on it, so its place says on which frame it was born. Wallpaper Engine adds rate x frame time to a
# single-precision count and emits its whole part: at 4 a second and 30 frames a second the births fall on frames
# 8, 15, 23, 30, ... (a double-precision count gives 8, 16, 23, 31, ...).
def stream(rate):
    return {
        "material": "materials/particle/halo.json", "maxcount": 64, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"}],
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "0 0 0", "rate": rate, "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 4, "max": 4},
            {"id": 3, "name": "sizerandom", "min": 16, "max": 16},
            {"id": 4, "name": "velocityrandom", "min": "200 0 0", "max": "200 0 0"},
        ],
        "operator": [{"id": 5, "name": "movement", "drag": 0, "gravity": "0 0 0"}],
        "renderer": [{"id": 90, "name": "sprite"}],
    }


w = Wallpaper('particles-emission-phase', 'emission timing of steady streams', clear='0 0 0')
for k, rate in enumerate((3, 4, 6, 7, 9, 12, 13, 17)):
    name = 'rate%d' % rate
    w.files['particles/%s.json' % name] = stream(rate)
    w.add({"name": name, "particle": "particles/%s.json" % name, "origin": "150 %d 0" % (960 - 95 * k),
           "scale": "1 1 1", "angles": "0 0 0", "visible": True})
w.save()
with open(os.path.join(HERE, 'particles-emission-phase', 'compare.json'), 'w') as f:
    json.dump({"probe": True, "fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'particles-emission-phase'))

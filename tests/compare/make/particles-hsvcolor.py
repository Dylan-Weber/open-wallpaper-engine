# particles-hsvcolor: the hsvcolorrandom initializer with no random spread (min = max): hues 0, 0.33, 0.6 and 0.85 at
# full saturation and value, then hue 0.6 at saturation 0.5, at value 0.5, and both.
def single(h, s, v):
    return {
        "material": "materials/particle/halo.json", "maxcount": 4, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"}],
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "0 0 0", "rate": 0, "instantaneous": 1,
                     "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 100000, "max": 100000},
            {"id": 3, "name": "sizerandom", "min": 120, "max": 120},
            {"id": 4, "name": "hsvcolorrandom", "huemin": h, "huemax": h, "saturationmin": s, "saturationmax": s,
             "valuemin": v, "valuemax": v},
        ],
        "operator": [],
        "renderer": [{"id": 90, "name": "sprite"}],
    }


cells = ((0, 1, 1), (0.33, 1, 1), (0.6, 1, 1), (0.85, 1, 1), (0.6, 0.5, 1), (0.6, 1, 0.5), (0.6, 0.5, 0.5))
w = Wallpaper('particles-hsvcolor', 'particle colours from hue, saturation and value', clear='0 0 0')
for k, (h, s, v) in enumerate(cells):
    w.files['particles/c%d.json' % k] = single(h, s, v)
    w.add({"name": "hsv %s %s %s" % (h, s, v), "particle": "particles/c%d.json" % k,
           "origin": "%d %d 0" % (300 + 330 * (k % 4), 800 - 300 * (k // 4)), "scale": "1 1 1", "angles": "0 0 0",
           "visible": True})
w.save()
clock_probe.stamp(os.path.join(HERE, 'particles-hsvcolor'))

# particles-hsvcolor-narrow: hsvcolorrandom with ranges too narrow to see (min and max a hair apart, so the colour is
# fixed; equal ends would pick any hue): hues 0.02, 0.3, 0.6 and 0.85 at full saturation and value, hue 0.6 at
# saturation 0.5, at value 0.5, and both.
def single(h, s, v):
    e = 0.0005
    return {
        "material": "materials/particle/halo.json", "maxcount": 4, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"}],
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "0 0 0", "rate": 0, "instantaneous": 1,
                     "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 100000, "max": 100000},
            {"id": 3, "name": "sizerandom", "min": 140, "max": 140},
            {"id": 4, "name": "hsvcolorrandom", "huemin": h, "huemax": h + e, "saturationmin": s - e,
             "saturationmax": s, "valuemin": v - e, "valuemax": v},
        ],
        "operator": [],
        "renderer": [{"id": 90, "name": "sprite"}],
    }


cells = ((0.02, 1, 1), (0.3, 1, 1), (0.6, 1, 1), (0.85, 1, 1), (0.6, 0.5, 1), (0.6, 1, 0.5), (0.6, 0.5, 0.5))
w = Wallpaper('particles-hsvcolor-narrow', 'particle colours from narrow hue, saturation and value ranges',
              clear='0 0 0')
for k, (h, s, v) in enumerate(cells):
    w.files['particles/c%d.json' % k] = single(h, s, v)
    w.add({"name": "hsv %s %s %s" % (h, s, v), "particle": "particles/c%d.json" % k,
           "origin": "%d %d 0" % (300 + 330 * (k % 4), 800 - 300 * (k // 4)), "scale": "1 1 1", "angles": "0 0 0",
           "visible": True})
w.save()
clock_probe.stamp(os.path.join(HERE, 'particles-hsvcolor-narrow'))

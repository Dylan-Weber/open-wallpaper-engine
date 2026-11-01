# particles-hsvrange: hsvcolorrandom with hue ranges: 100 particles each, hue 0.5..0.6, hue 0.1..0.15 with 3 steps,
# and saturation 0.2..1 at hue 0..0.05; their colours show which hues Wallpaper Engine picks.
def burst(init):
    return {
        "material": "materials/particle/halo.json", "maxcount": 120, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"}],
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "250 120 0",
                     "directions": "1 1 0", "origin": "0 0 0", "rate": 0, "instantaneous": 100,
                     "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 100000, "max": 100000},
            {"id": 3, "name": "sizerandom", "min": 14, "max": 14},
            dict(init, id=4, name="hsvcolorrandom"),
        ],
        "operator": [],
        "renderer": [{"id": 90, "name": "sprite"}],
    }


cells = (("range", {"huemin": 0.5, "huemax": 0.6, "saturationmin": 1, "saturationmax": 1, "valuemin": 1,
                    "valuemax": 1}),
         ("steps", {"huemin": 0.1, "huemax": 0.15, "huesteps": 3, "saturationmin": 1, "saturationmax": 1,
                    "valuemin": 1, "valuemax": 1}),
         ("saturation", {"huemin": 0.0, "huemax": 0.05, "saturationmin": 0.2, "saturationmax": 1, "valuemin": 1,
                         "valuemax": 1}))
w = Wallpaper('particles-hsvrange', 'particle colours from hue ranges', clear='0 0 0')
for k, (name, init) in enumerate(cells):
    w.files['particles/%s.json' % name] = burst(init)
    w.add({"name": name, "particle": "particles/%s.json" % name, "origin": "%d 600 0" % (330 + 620 * k),
           "scale": "1 1 1", "angles": "0 0 0", "visible": True})
w.save()
clock_probe.stamp(os.path.join(HERE, 'particles-hsvrange'))

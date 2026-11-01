# particles-ropetrail: the ropetrail renderer draws a ribbon along each moving particle's recent path: particles
# emitted every two seconds at 100 units/s to the right (no drag), living 4 s, with trail length 0.25, 0.5 and 1,
# at 30 frames a second.
def trail(length):
    return {
        "material": "materials/particle/halo.json", "maxcount": 20, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"}],
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "0 0 0", "rate": 0.5, "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 4, "max": 4},
            {"id": 3, "name": "sizerandom", "min": 30, "max": 30},
            {"id": 4, "name": "colorrandom", "min": "255 200 80", "max": "255 200 80"},
            {"id": 5, "name": "velocityrandom", "min": "100 0 0", "max": "100 0 0"},
        ],
        "operator": [{"id": 20, "name": "movement", "drag": 0, "gravity": "0 0 0"}],
        "renderer": [{"id": 90, "name": "ropetrail", "length": length}],
    }


w = Wallpaper('particles-ropetrail', 'ropetrail renderer: trail length', clear='0 0 0')
for k, length in enumerate((0.25, 0.5, 1.0)):
    name = 'trail%d' % k
    w.files['particles/%s.json' % name] = trail(length)
    w.add({"name": name, "particle": "particles/%s.json" % name, "origin": "100 %d 0" % (840 - 250 * k),
           "scale": "1 1 1", "angles": "0 0 0", "visible": True})
w.save()
with open(os.path.join(HERE, 'particles-ropetrail', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'particles-ropetrail'))

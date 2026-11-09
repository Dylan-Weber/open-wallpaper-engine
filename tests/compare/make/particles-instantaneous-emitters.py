# particles-instantaneous-emitters: systems with several instantaneous emitters (a burst each when the system has no
# particles). Wallpaper Engine fires every one of them; owe fired only the first that ran.
def cell(points, burst=1):
    return {
        "material": "materials/particle/halo.json", "maxcount": 16, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"}],
        "emitter": [{"id": 10 + k, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "%g %g 0" % p, "rate": 0, "instantaneous": burst,
                     "speedmin": 0, "speedmax": 0} for k, p in enumerate(points)],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 100000, "max": 100000},
            {"id": 3, "name": "sizerandom", "min": 40, "max": 40},
        ],
        "operator": [{"id": 5, "name": "movement"}],
        "renderer": [{"id": 90, "name": "sprite"}],
    }


cells = [cell([(-60, 0), (60, 0)]), cell([(-80, 0), (0, 0), (80, 0)]), cell([(0, -60), (0, 60)], burst=2),
         cell([(-60, -60), (60, -60), (-60, 60), (60, 60)])]
w = Wallpaper('particles-instantaneous-emitters', 'several instantaneous emitters in one system', clear='0 0 0')
for k, c in enumerate(cells):
    w.files['particles/c%d.json' % k] = c
    w.add({"name": "burst %d" % k, "particle": "particles/c%d.json" % k,
           "origin": "%d %d 0" % (300 + 440 * k, 650), "scale": "1 1 1", "angles": "0 0 0", "visible": True})
w.save()
clock_probe.stamp(os.path.join(HERE, 'particles-instantaneous-emitters'))

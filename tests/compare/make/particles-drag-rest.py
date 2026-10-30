# particles-drag-rest: single particles thrown to the right under drag, long enough to have come to rest: with
# Wallpaper Engine's drag (speed falls as e^(-drag t / 2)) a particle thrown at drag/2 x D units/s stops D units
# away. Rows drag 0.2, 0.5, 1 and 2; particles meant to stop 300, 600 and 900 units out.
def thrown(drag, distance):
    speed = drag / 2 * distance
    return {
        "material": "materials/particle/halo.json", "maxcount": 4, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"}],
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "0 0 0", "rate": 0, "instantaneous": 1,
                     "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 100000, "max": 100000},
            {"id": 3, "name": "sizerandom", "min": 50, "max": 50},
            {"id": 4, "name": "colorrandom", "min": "255 220 80", "max": "255 220 80"},
            {"id": 5, "name": "velocityrandom", "min": "%g 0 0" % speed, "max": "%g 0 0" % speed},
        ],
        "operator": [{"id": 20, "name": "movement", "drag": drag, "gravity": "0 0 0"}],
        "renderer": [{"id": 90, "name": "sprite"}],
    }


w = Wallpaper('particles-drag-rest', 'particles come to rest under drag', clear='0 0 0')
for k, drag in enumerate((0.2, 0.5, 1, 2)):
    for j, distance in enumerate((300, 600, 900)):
        name = 'drag%d_%d' % (k, j)
        w.files['particles/%s.json' % name] = thrown(drag, distance)
        w.add({"name": name, "particle": "particles/%s.json" % name,
               "origin": "250 %d 0" % (920 - 170 * k - 40 * j), "scale": "1 1 1", "angles": "0 0 0",
               "visible": True})
w.save()
clock_probe.stamp(os.path.join(HERE, 'particles-drag-rest'))

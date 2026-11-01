# particles-drag: how drag slows particles: streams emitted 4 a second at 200 units/s to the right, living 3 s, with
# drag 0.2, 0.5, 1 and 2 (no random spread), so each row's dots show distance against age.
def stream(drag):
    return {
        "material": "materials/particle/halo.json", "maxcount": 40, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"}],
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "0 0 0", "rate": 4, "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 3, "max": 3},
            {"id": 3, "name": "sizerandom", "min": 40, "max": 40},
            {"id": 4, "name": "colorrandom", "min": "255 220 80", "max": "255 220 80"},
            {"id": 5, "name": "velocityrandom", "min": "200 0 0", "max": "200 0 0"},
        ],
        "operator": [{"id": 20, "name": "movement", "drag": drag, "gravity": "0 0 0"}],
        "renderer": [{"id": 90, "name": "sprite"}],
    }


w = Wallpaper('particles-drag', 'particle drag', clear='0 0 0')
for k, drag in enumerate((0.2, 0.5, 1, 2)):
    w.files['particles/drag%d.json' % k] = stream(drag)
    w.add({"name": "drag %s" % drag, "particle": "particles/drag%d.json" % k, "origin": "200 %d 0" % (900 - 160 * k),
           "scale": "1 1 1", "angles": "0 0 0", "visible": True})
w.save()
with open(os.path.join(HERE, 'particles-drag', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'particles-drag'))

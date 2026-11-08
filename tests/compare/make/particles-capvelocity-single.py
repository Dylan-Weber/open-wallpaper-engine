# particles-capvelocity-single: single particles under capvelocity, so their place is cap x time: thrown at 400
# units/s capped at 40, and thrown up and right at 300 capped at 60. Without the cap they fly off the screen.
def single(velocity, cap, gravity="0 0 0"):
    return {
        "material": "materials/particle/halo.json", "maxcount": 4, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"}],
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "0 0 0", "rate": 0, "instantaneous": 1,
                     "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 100000, "max": 100000},
            {"id": 3, "name": "sizerandom", "min": 160, "max": 160},
            {"id": 4, "name": "colorrandom", "min": "255 220 80", "max": "255 220 80"},
            {"id": 5, "name": "velocityrandom", "min": velocity, "max": velocity},
        ],
        "operator": [{"id": 20, "name": "movement", "drag": 0, "gravity": gravity},
                     {"id": 21, "name": "capvelocity", "maxspeed": cap}],
        "renderer": [{"id": 90, "name": "sprite"}],
    }


w = Wallpaper('particles-capvelocity-single', 'single particles under a speed cap', clear='0 0 0')
for name, system, origin in (('fast', single("400 0 0", 40), "200 900 0"),
                             ('diagonal', single("240 180 0", 60), "200 200 0")):
    w.files['particles/%s.json' % name] = system
    w.add({"name": name, "particle": "particles/%s.json" % name, "origin": origin, "scale": "1 1 1",
           "angles": "0 0 0", "visible": True})
w.save()
with open(os.path.join(HERE, 'particles-capvelocity-single', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'particles-capvelocity-single'))

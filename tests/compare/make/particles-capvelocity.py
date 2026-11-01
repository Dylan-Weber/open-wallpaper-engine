# particles-capvelocity: the capvelocity operator limits a particle's speed: streams emitted 4 a second at 200 units/s
# to the right (no random spread), living 3 s, without a cap, capped at 50 and at 100; and a stream falling from rest
# under gravity 300, capped at 80.
def stream(operators, velocity="200 0 0"):
    return {
        "material": "materials/particle/halo.json", "maxcount": 40, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"}],
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "0 0 0", "rate": 4, "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 3, "max": 3},
            {"id": 3, "name": "sizerandom", "min": 40, "max": 40},
            {"id": 4, "name": "colorrandom", "min": "255 220 80", "max": "255 220 80"},
            {"id": 5, "name": "velocityrandom", "min": velocity, "max": velocity},
        ],
        "operator": operators,
        "renderer": [{"id": 90, "name": "sprite"}],
    }


move = {"id": 20, "name": "movement", "drag": 0, "gravity": "0 0 0"}
rows = (('free', stream([move]), "200 900 0"),
        ('cap50', stream([move, {"id": 21, "name": "capvelocity", "maxspeed": 50}]), "200 740 0"),
        ('cap100', stream([move, {"id": 21, "name": "capvelocity", "maxspeed": 100}]), "200 580 0"),
        ('fall', stream([{"id": 20, "name": "movement", "drag": 0, "gravity": "300 0 0"},
                         {"id": 21, "name": "capvelocity", "maxspeed": 80}], velocity="0 0 0"), "200 420 0"))
w = Wallpaper('particles-capvelocity', 'particle speed caps', clear='0 0 0')
for name, system, origin in rows:
    w.files['particles/%s.json' % name] = system
    w.add({"name": name, "particle": "particles/%s.json" % name, "origin": origin, "scale": "1 1 1",
           "angles": "0 0 0", "visible": True})
w.save()
with open(os.path.join(HERE, 'particles-capvelocity', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'particles-capvelocity'))

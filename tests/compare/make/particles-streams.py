# particles-streams: particles that move by fixed rules, so the picture depends only on time: streams emitted 4 a
# second at 200 units/s to the right, living 3 s, one row each for movement alone, movement with drag, gravity, alpha
# fade, size change, and colour change (operators with fixed settings, no random spread).
def stream(operators, size=24, color='255 220 80'):
    return {
        "material": "materials/particle/halo.json", "maxcount": 40, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"}],
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "0 0 0", "rate": 4, "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 3, "max": 3},
            {"id": 3, "name": "sizerandom", "min": size, "max": size},
            {"id": 4, "name": "colorrandom", "min": color, "max": color},
            {"id": 5, "name": "velocityrandom", "min": "200 0 0", "max": "200 0 0"},
        ],
        "operator": operators,
        "renderer": [{"id": 90, "name": "sprite"}],
    }


rows = (('movement', [{"id": 20, "name": "movement", "drag": 0, "gravity": "0 0 0"}]),
        ('drag', [{"id": 20, "name": "movement", "drag": 0.5, "gravity": "0 0 0"}]),
        ('gravity', [{"id": 20, "name": "movement", "drag": 0, "gravity": "0 -60 0"}]),
        ('alphafade', [{"id": 20, "name": "movement", "drag": 0, "gravity": "0 0 0"},
                       {"id": 21, "name": "alphafade", "fadeintime": 0.3, "fadeouttime": 0.5}]),
        ('sizechange', [{"id": 20, "name": "movement", "drag": 0, "gravity": "0 0 0"},
                        {"id": 21, "name": "sizechange", "startvalue": 0.3, "endvalue": 2.5}]),
        ('colorchange', [{"id": 20, "name": "movement", "drag": 0, "gravity": "0 0 0"},
                         {"id": 21, "name": "colorchange", "startvalue": "1 0 0", "endvalue": "0 0.5 1"}]))
w = Wallpaper('particles-streams', 'particle streams under fixed operators', clear='0 0 0')
for k, (name, operators) in enumerate(rows):
    w.files['particles/%s.json' % name] = stream(operators)
    w.add({"name": name, "particle": "particles/%s.json" % name, "origin": "200 %d 0" % (960 - 150 * k),
           "scale": "1 1 1", "angles": "0 0 0", "visible": True})
w.save()
with open(os.path.join(HERE, 'particles-streams', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'particles-streams'))

# particles-rope: the rope renderer joins a system's particles in order into a ribbon through them; the ropetrail
# renderer stretches each along its velocity. Static particles: 10 on a line between two control points with rope
# (default subdivision) and with subdivision 3; 10 around a ring (curved, so the subdivision shows) with rope
# subdivision 4 and 100; and 4 still particles with velocities and ropetrail length 0.2.
def system(initializers, renderer, count, size=36, emitter="boxrandom", distance="0 0 0"):
    return {
        "material": "materials/particle/halo.json", "maxcount": 40, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"}, {"id": 1, "flags": 0, "offset": "420 0 0"}],
        "emitter": [{"id": 1, "name": emitter, "distancemin": distance, "distancemax": distance,
                     "directions": "1 1 0", "origin": "0 0 0", "rate": 0, "instantaneous": count,
                     "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 100000, "max": 100000},
            {"id": 3, "name": "sizerandom", "min": size, "max": size},
            {"id": 4, "name": "colorrandom", "min": "255 200 80", "max": "255 200 80"},
        ] + [dict(i, id=10 + n) for n, i in enumerate(initializers)],
        "operator": [],
        "renderer": [dict(renderer, id=90)],
    }


line = [{"name": "mapsequencebetweencontrolpoints", "controlpointstart": 0, "controlpointend": 1, "count": 10}]
ring = [{"name": "mapsequencearoundcontrolpoint", "controlpoint": 0, "count": 10, "speedmin": "0 0 0",
         "speedmax": "0 0 0"}]
cells = (('line', system(line, {"name": "rope"}, 10), "150 880 0", "1 1 1"),
         ('line sub 3', system(line, {"name": "rope", "subdivision": 3}, 10), "150 640 0", "1 1 1"),
         ('ring sub 4', system(ring, {"name": "rope", "subdivision": 4}, 10, emitter="sphererandom", distance=120),
          "1250 800 0", "1 1 1"),
         ('ring sub 100', system(ring, {"name": "rope", "subdivision": 100}, 10, emitter="sphererandom",
                                 distance=120), "1250 400 0", "1 1 1"))
w = Wallpaper('particles-rope', 'rope and ropetrail renderers', clear='0 0 0')
for k, (label, s, origin, scale) in enumerate(cells):
    name = 'rope%d' % k
    w.files['particles/%s.json' % name] = s
    w.add({"name": name, "particle": "particles/%s.json" % name, "origin": origin, "scale": scale, "angles": "0 0 0",
           "visible": True})
for i, velocity in enumerate(("60 0 0", "150 60 0", "0 220 0", "-120 -120 0")):
    s = system([{"name": "mapsequencebetweencontrolpoints", "controlpointstart": 0, "controlpointend": 1, "count": 5},
                {"name": "velocityrandom", "min": velocity, "max": velocity}],
               {"name": "ropetrail", "length": 0.2}, 5, size=20)
    s["controlpoint"][1]["offset"] = "0 120 0"
    name = 'trail%d' % i
    w.files['particles/%s.json' % name] = s
    w.add({"name": name, "particle": "particles/%s.json" % name, "origin": "%d 160 0" % (300 + 340 * i),
           "scale": "1 1 1", "angles": "0 0 0", "visible": True})
w.save()
with open(os.path.join(HERE, 'particles-rope', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'particles-rope'))

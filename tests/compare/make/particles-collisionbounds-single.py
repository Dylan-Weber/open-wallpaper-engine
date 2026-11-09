# particles-collisionbounds-single: one particle per system, thrown at the scene's edges (collisionbounds: the scene is
# 0..1920 x 0..1080) with no gravity and no drag, so where it ends up says what the bounce did: an off-axis hit on the
# right edge, a hit on the top edge, and a corner hit, for bounce 0.5, bounce 0, slide, delete and stop.
def cell(position, velocity, extra):
    op = {"id": 6, "name": "collisionbounds"}
    op.update(extra)
    return {
        "material": "materials/particle/halo.json", "maxcount": 4, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"}],
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0", "directions": "1 1 0",
                     "origin": "0 0 0", "rate": 0, "instantaneous": 1, "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 100000, "max": 100000},
            {"id": 3, "name": "sizerandom", "min": 30, "max": 30},
            {"id": 4, "name": "velocityrandom", "min": velocity, "max": velocity},
        ],
        "operator": [{"id": 5, "name": "movement", "drag": 0, "gravity": "0 0 0"}, op],
        "renderer": [{"id": 90, "name": "sprite"}],
        "_origin": position,
    }


behaviours = [{"bouncefactor": 0.5}, {"bouncefactor": 0}, {"collisionbehavior": "slide"},
              {"collisionbehavior": "delete"}, {"collisionbehavior": "stop"}]
hits = [("1800 300 0", "60 20 0"), ("400 1000 0", "15 40 0"), ("1840 1000 0", "40 40 0")]
w = Wallpaper('particles-collisionbounds-single', 'single particles bouncing off the scene edges', clear='0 0 0')
k = 0
for b in behaviours:
    for j, (position, velocity) in enumerate(hits):
        c = cell(position, velocity, b)
        origin = c.pop("_origin")
        # each behaviour on its own row of starting points (shifted down) so the cells don't overlap
        x, y, z = (float(v) for v in origin.split())
        y -= 150 * behaviours.index(b) if j != 1 else 0
        x += -280 * behaviours.index(b) if j == 1 else 0
        w.files['particles/c%d.json' % k] = c
        w.add({"name": "hit %d" % k, "particle": "particles/c%d.json" % k, "origin": "%g %g 0" % (x, y),
               "scale": "1 1 1", "angles": "0 0 0", "visible": True})
        k += 1
w.save()
with open(os.path.join(HERE, 'particles-collisionbounds-single', 'compare.json'), 'w') as f:
    json.dump({"probe": True, "fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'particles-collisionbounds-single'))

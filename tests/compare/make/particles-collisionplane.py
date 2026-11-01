# particles-collisionplane: the collisionplane operator. A stream of particles thrown up and right at 250 400 under
# gravity -500; a plane at distance -100 (default normal up). Cells: bounce 0.7, bounce 0.2, behavior delete, behavior
# slide, then a "plane" key of 1 0 0 with a particle thrown left.
def cell(collision, velocity="250 400 0"):
    return {
        "material": "materials/particle/halo.json", "maxcount": 200, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"}],
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "0 0 0", "rate": 20, "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 4, "max": 4},
            {"id": 3, "name": "sizerandom", "min": 24, "max": 24},
            {"id": 4, "name": "velocityrandom", "min": velocity, "max": velocity},
        ],
        "operator": [{"id": 5, "name": "movement", "gravity": "0 -500 0", "drag": 0},
                     dict(collision, id=6, name="collisionplane")],
        "renderer": [{"id": 90, "name": "sprite"}],
    }


cells = (
    {"distance": -100, "bouncefactor": 0.7},
    {"distance": -100, "bouncefactor": 0.2},
    {"distance": -100, "bouncefactor": 0.7, "collisionbehavior": "delete"},
    {"distance": -100, "bouncefactor": 0.7, "collisionbehavior": "slide"},
    {"distance": -100, "bouncefactor": 0.7, "plane": "1 0 0"},
    {"distance": 100, "bouncefactor": 0.7},
)
w = Wallpaper('particles-collisionplane', 'particles bouncing off a plane', clear='0 0 0')
for k, collision in enumerate(cells):
    w.files['particles/c%d.json' % k] = cell(collision, "-250 300 0" if k == 4 else "250 400 0")
    w.add({"name": "plane %d" % k, "particle": "particles/c%d.json" % k,
           "origin": "%d %d 0" % (200 + 570 * (k % 3), 750 - 450 * (k // 3)), "scale": "1 1 1", "angles": "0 0 0",
           "visible": True})
w.save()
clock_probe.stamp(os.path.join(HERE, 'particles-collisionplane'))

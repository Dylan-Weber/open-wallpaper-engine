# particles-child-static: when a "static" child system's instantaneous emitter fires (thunderbolt's glow child: a halo
# that lives 0.1..0.2 s). The child is a big faint halo (instantaneous 1, life 0.3 s). Parents: (A) one particle every
# 2 s moving right at 100 units/s (life 1.5 s); (B) no particles at all; (C) periodic like thunderbolt (flag 4, 1 s on,
# 1 s off, rate 5). A series of shots shows when and where the halo appears.
def glow():
    return {
        "material": "materials/particle/halo.json", "maxcount": 4, "starttime": 0, "flags": 0,
        "emitter": [{"id": 1, "name": "sphererandom", "distancemin": 0, "distancemax": 0, "rate": 0, "instantaneous": 1}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 0.3, "max": 0.3},
            {"id": 3, "name": "sizerandom", "min": 120, "max": 120},
            {"id": 4, "name": "colorrandom", "min": "255 120 60", "max": "255 120 60"},
        ],
        "operator": [{"id": 5, "name": "movement", "drag": 0}],
        "renderer": [{"id": 90, "name": "sprite"}],
    }


def parent(emitter):
    return {
        "material": "materials/particle/halo.json", "maxcount": 8, "starttime": 0, "flags": 0,
        "emitter": [emitter] if emitter else [],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 1.5, "max": 1.5},
            {"id": 3, "name": "sizerandom", "min": 20, "max": 20},
            {"id": 4, "name": "velocityrandom", "min": "100 0 0", "max": "100 0 0"},
        ],
        "operator": [{"id": 5, "name": "movement", "drag": 0, "gravity": "0 0 0"}],
        "renderer": [{"id": 90, "name": "sprite"}],
        "children": [{"id": 9, "maxcount": 10, "name": "particles/glow.json", "type": "static", "flags": 2,
                      "probability": 1.0, "origin": "0 0 0", "angles": "0 0 0", "scale": "1 1 1"}],
    }


base = {"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0", "directions": "1 1 0",
        "origin": "0 0 0", "speedmin": 0, "speedmax": 0}
parents = (
    ("A every 2 s", dict(base, rate=0.5)),
    ("B none", None),
    ("C periodic", dict(base, rate=5, flags=4, minperiodicdelay=1, maxperiodicdelay=1, minperiodicduration=1,
                        maxperiodicduration=1)),
)
name = 'particles-child-static'
w = Wallpaper(name, 'when a static child system fires: thunderbolt glow', clear='0 0 0')
w.files['particles/glow.json'] = glow()
for k, (label, emitter) in enumerate(parents):
    w.files['particles/p%d.json' % k] = parent(emitter)
    w.add({"name": label, "particle": "particles/p%d.json" % k,
           "origin": "%d %d 0" % (250 + 600 * k, 600), "scale": "1 1 1", "angles": "0 0 0"})
w.save()
clock_probe.stamp(os.path.join(HERE, name))
write(name + '/compare.json', {"probe": True, "fps": 30, "shots": 40, "shot_interval_ms": 100})

# particles-spritetrail: the spritetrail renderer stretches a sprite along its velocity. Particles that stand still
# (no movement operator) with velocities of 0, 50, 150 and 400 units/s to the right and 150 diagonally, in rows with
# length 0.05, length 0.01, length 0.05 with maxlength 3 and with minlength 2 / maxlength 8.
def row(renderer):
    count = 5
    return {
        "material": "materials/particle/halo.json", "maxcount": 10, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"}, {"id": 1, "flags": 0, "offset": "800 0 0"}],
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "0 0 0", "rate": 0, "instantaneous": count,
                     "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 100000, "max": 100000},
            {"id": 3, "name": "sizerandom", "min": 24, "max": 24},
            {"id": 4, "name": "colorrandom", "min": "255 200 80", "max": "255 200 80"},
            {"id": 5, "name": "mapsequencebetweencontrolpoints", "controlpointstart": 0, "controlpointend": 1,
             "count": count},
            {"id": 6, "name": "velocityrandom", "min": "0 0 0", "max": "0 0 0"},
        ],
        "operator": [],
        "renderer": [dict(renderer, id=90, name="spritetrail")],
    }


rows = (('length 0.05', {"length": 0.05}), ('length 0.01', {"length": 0.01}),
        ('maxlength 3', {"length": 0.05, "maxlength": 3}),
        ('min 2 max 8', {"length": 0.05, "minlength": 2, "maxlength": 8}))
w = Wallpaper('particles-spritetrail', 'spritetrail renderer: length, minlength and maxlength', clear='0 0 0')
for k, (label, renderer) in enumerate(rows):
    system = row(renderer)
    # five particles in a row, each with its own speed: the velocity initializers are chosen by the sequence index,
    # so the particles are made one per system: five systems per row
    for i, velocity in enumerate(("0 0 0", "50 0 0", "150 0 0", "400 0 0", "106 106 0")):
        s = json.loads(json.dumps(system))
        s["emitter"][0]["instantaneous"] = 1
        s["initializer"] = [i2 for i2 in s["initializer"] if i2["name"] != "mapsequencebetweencontrolpoints"]
        s["initializer"][-1] = {"id": 6, "name": "velocityrandom", "min": velocity, "max": velocity}
        name = 'trail_%d_%d' % (k, i)
        w.files['particles/%s.json' % name] = s
        w.add({"name": name, "particle": "particles/%s.json" % name,
               "origin": "%d %d 0" % (260 + 340 * i, 900 - 220 * k), "scale": "1 1 1", "angles": "0 0 0",
               "visible": True})
w.save()
with open(os.path.join(HERE, 'particles-spritetrail', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'particles-spritetrail'))

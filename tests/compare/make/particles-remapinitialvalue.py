# particles-remapinitialvalue: the remapinitialvalue initializer maps an input onto a particle value once, when it
# is born. Static rows of 9 particles from control point 0 to 400 units out remap the distance to control point 0
# (input range 0 to 400) onto colour red to blue and onto size 10 to 80; and a particle born at control point 0 moving
# right at 40 units/s with colour remapped from distance: it stays the colour it was born with.
def system(initializer, static=True, size=40):
    inits = [
        {"id": 2, "name": "lifetimerandom", "min": 100000, "max": 100000},
        {"id": 3, "name": "sizerandom", "min": size, "max": size},
        {"id": 4, "name": "colorrandom", "min": "255 255 255", "max": "255 255 255"},
    ]
    if static:
        inits.append({"id": 5, "name": "mapsequencebetweencontrolpoints", "controlpointstart": 0,
                      "controlpointend": 1, "count": 9})
    else:
        inits.append({"id": 5, "name": "velocityrandom", "min": "40 0 0", "max": "40 0 0"})
    inits.append(dict(initializer, id=6, name="remapinitialvalue", input="distancetocontrolpoint",
                      inputcontrolpoint0=0, inputrangemin=0, inputrangemax=400))
    return {
        "material": "materials/particle/halo.json", "maxcount": 30, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"}, {"id": 1, "flags": 0, "offset": "400 0 0"}],
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "0 0 0", "rate": 0,
                     "instantaneous": 9 if static else 1, "speedmin": 0, "speedmax": 0}],
        "initializer": inits,
        "operator": [] if static else [{"id": 20, "name": "movement", "drag": 0, "gravity": "0 0 0"}],
        "renderer": [{"id": 90, "name": "sprite"}],
    }


rows = (('color', system({"operation": "remap", "output": "color", "outputrangemin": "1 0 0",
                          "outputrangemax": "0 0 1"})),
        ('size', system({"operation": "remap", "output": "size", "outputrangemin": 10, "outputrangemax": 80})),
        ('moving', system({"operation": "remap", "output": "color", "outputrangemin": "1 0 0",
                           "outputrangemax": "0 0 1"}, static=False)))
w = Wallpaper('particles-remapinitialvalue', 'particle values remapped at birth', clear='0 0 0')
for k, (name, s) in enumerate(rows):
    w.files['particles/%s.json' % name] = s
    w.add({"name": name, "particle": "particles/%s.json" % name, "origin": "500 %d 0" % (860 - 200 * k),
           "scale": "1 1 1", "angles": "0 0 0", "visible": True})
w.save()
clock_probe.stamp(os.path.join(HERE, 'particles-remapinitialvalue'))

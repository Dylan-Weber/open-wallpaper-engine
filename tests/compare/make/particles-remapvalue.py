# particles-remapvalue: the remapvalue operator maps an input onto a particle value. Static rows of 9 particles from
# control point 0 to 400 units out (mapsequencebetweencontrolpoints), each remapping the distance to control point 0
# (input range 0 to 400): colour red to blue, size 10 to 80, opacity 1 to 0.1, and size multiplied by 0.2 to 2.
def row(operator, size=40, color='255 255 255'):
    return {
        "material": "materials/particle/halo.json", "maxcount": 20, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"}, {"id": 1, "flags": 0, "offset": "400 0 0"}],
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "0 0 0", "rate": 0, "instantaneous": 9,
                     "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 100000, "max": 100000},
            {"id": 3, "name": "sizerandom", "min": size, "max": size},
            {"id": 4, "name": "colorrandom", "min": color, "max": color},
            {"id": 5, "name": "mapsequencebetweencontrolpoints", "controlpointstart": 0, "controlpointend": 1,
             "count": 9},
        ],
        "operator": [dict(operator, id=20, name="remapvalue", input="distancetocontrolpoint", inputcontrolpoint0=0,
                          inputrangemin=0, inputrangemax=400)],
        "renderer": [{"id": 90, "name": "sprite"}],
    }


rows = (('color', row({"operation": "remap", "output": "color", "outputrangemin": "1 0 0", "outputrangemax": "0 0 1"})),
        ('size', row({"operation": "remap", "output": "size", "outputrangemin": 10, "outputrangemax": 80})),
        ('opacity', row({"operation": "remap", "output": "opacity", "outputrangemin": 1, "outputrangemax": 0.1})),
        ('multiply', row({"operation": "multiply", "output": "size", "outputrangemin": 0.2, "outputrangemax": 2})))
w = Wallpaper('particles-remapvalue', 'particle values remapped from distance', clear='0 0 0')
for k, (name, system) in enumerate(rows):
    w.files['particles/%s.json' % name] = system
    w.add({"name": name, "particle": "particles/%s.json" % name, "origin": "500 %d 0" % (880 - 180 * k),
           "scale": "1 1 1", "angles": "0 0 0", "visible": True})
w.save()
clock_probe.stamp(os.path.join(HERE, 'particles-remapvalue'))

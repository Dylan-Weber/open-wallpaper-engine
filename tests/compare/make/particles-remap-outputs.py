# particles-remap-outputs: remapvalue's outputs and operations. One particle per cell, living 20 s, whose lifetime fraction
# (0 to 1) is mapped onto a range; at the capture (~11.3 s) the fraction is 0.56. Colour cells start from the colour
# 1 0.5 0.25 so that "set" and "multiply" differ; size cells show the size; velocity cells (movement on) show how far it
# went.
def cell(remap, initializers=()):
    return {
        "material": "materials/particle/halo.json", "maxcount": 4, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"}],
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "-100 0 0", "rate": 0, "instantaneous": 1,
                     "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 20, "max": 20},
            {"id": 3, "name": "sizerandom", "min": 60, "max": 60},
            {"id": 4, "name": "colorrandom", "min": "255 128 64", "max": "255 128 64"},
        ] + list(initializers),
        "operator": [{"id": 5, "name": "movement"},
                     dict({"id": 6, "name": "remapvalue", "input": "lifetimefraction", "inputrangemin": 0,
                           "inputrangemax": 1}, **remap)],
        "renderer": [{"id": 90, "name": "sprite"}],
    }


grey = {"outputrangemin": "0 0 0", "outputrangemax": "1 1 1"}
cells = (
    ("velocity remap", dict(output="velocity", operation="remap", outputrangemin="0 0 0",
                            outputrangemax="200 0 0")),
    ("velocity add", dict(output="velocity", operation="add", outputrangemin="0 0 0", outputrangemax="20 0 0")),
    ("subtract", dict(output="color", operation="subtract", **grey)),
    ("triangle", dict(output="color", operation="triangle", **grey)),
    ("sine", dict(output="color", operation="sine", **grey)),
    ("square", dict(output="color", operation="square", **grey)),
    ("average", dict(output="color", operation="average", **grey)),
    ("simplexnoise", dict(output="color", operation="simplexnoise", **grey)),
    ("fbmnoise", dict(output="color", operation="fbmnoise", **grey)),
    ("transform triangle", dict(output="color", operation="remap", transformfunction="triangle", **grey)),
    ("transform sine", dict(output="color", operation="remap", transformfunction="sine", **grey)),
    ("transform square", dict(output="color", operation="remap", transformfunction="square", **grey)),
)
w = Wallpaper('particles-remap-outputs', 'remapvalue outputs', clear='0 0 0')
for k, (name, remap) in enumerate(cells):
    w.files['particles/c%d.json' % k] = cell(remap)
    w.add({"name": name, "particle": "particles/c%d.json" % k,
           "origin": "%d %d 0" % (340 + 440 * (k % 4), 850 - 250 * (k // 4)), "scale": "1 1 1", "angles": "0 0 0",
           "visible": True})
w.save()
clock_probe.stamp(os.path.join(HERE, 'particles-remap-outputs'))

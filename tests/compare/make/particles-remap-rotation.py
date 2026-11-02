# particles-remap-rotation: remapvalue's rotation and angularvelocity outputs. One particle per cell, at rest, living 20 s,
# drawn with an asymmetric texture (orientation_debug) so its turn can be read; at the capture (~11.3 s) the lifetime
# fraction is 0.56. Rotation cells map the fraction onto a range (radians or degrees? one number or the z of a vector?);
# angular velocity cells run `angularmovement` so a set angular velocity turns the particle for the 11 seconds.
w = None


def cell(remap, initializers=(), angular=False):
    operators = [{"id": 5, "name": "movement"}]
    if angular:
        operators.append({"id": 7, "name": "angularmovement"})
    operators.append(dict({"id": 6, "name": "remapvalue", "input": "lifetimefraction", "inputrangemin": 0,
                           "inputrangemax": 1}, **remap))
    return {
        "material": "materials/particle/orientation.json", "maxcount": 4, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"}],
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "0 0 0", "rate": 0, "instantaneous": 1,
                     "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 20, "max": 20},
            {"id": 3, "name": "sizerandom", "min": 120, "max": 120},
        ] + list(initializers),
        "operator": operators,
        "renderer": [{"id": 90, "name": "sprite"}],
    }


def rot(a, b, **extra):
    return dict({"output": "rotation", "operation": "remap", "outputrangemin": "0 0 %s" % a,
                 "outputrangemax": "0 0 %s" % b}, **extra)


def spin(a, b, **extra):
    return dict({"output": "angularvelocity", "operation": "remap", "outputrangemin": "0 0 %s" % a,
                 "outputrangemax": "0 0 %s" % b}, **extra)


start = [{"id": 4, "name": "rotationrandom", "min": "0 0 1", "max": "0 0 1"}]
turning = [{"id": 4, "name": "angularvelocityrandom", "min": "0 0 1", "max": "0 0 1"}]
cells = (
    ("no remap", None, [], False),
    ("rotation 0 to 1", rot(0, 1), [], False),
    ("rotation 0 to 3.14", rot(0, 3.14), [], False),
    ("rotation 0 to 90", rot(0, 90), [], False),
    ("rotation 0 to 360", rot(0, 360), [], False),
    ("rotation scalar 0 to 3.14", dict(output="rotation", operation="remap", outputrangemin=0, outputrangemax=3.14),
     [], False),
    ("rotation x 0 to 3.14", dict(output="rotation", operation="remap", outputrangemin="0 0 0",
                                   outputrangemax="3.14 0 0"), [], False),
    ("rotation add from 1", rot(0, 3.14, operation="add"), start, False),
    ("rotation multiply from 1", rot(0, 3.14, operation="multiply"), start, False),
    ("angular velocity set 1", spin(1, 1), [], True),
    ("angular velocity add 1 on 1", spin(1, 1, operation="add"), turning, True),
    ("angular velocity multiply 2 on 1", spin(2, 2, operation="multiply"), turning, True),
)
w = Wallpaper('particles-remap-rotation', 'remapvalue rotation outputs', clear='0 0 0')
w.files['materials/particle/orientation.json'] = {"passes": [{
    "shader": "genericparticle", "depthtest": "disabled", "depthwrite": "disabled", "blending": "translucent",
    "cullmode": "nocull", "textures": ["particle/orientation_debug"]}]}
for k, (name, remap, initializers, angular) in enumerate(cells):
    body = cell(remap or {"output": "color", "operation": "multiply", "outputrangemin": "1 1 1",
                          "outputrangemax": "1 1 1"}, initializers, angular)
    w.files['particles/c%d.json' % k] = body
    w.add({"name": name, "particle": "particles/c%d.json" % k,
           "origin": "%d %d 0" % (340 + 440 * (k % 4), 850 - 250 * (k // 4)), "scale": "1 1 1", "angles": "0 0 0",
           "visible": True})
w.save()
clock_probe.stamp(os.path.join(HERE, 'particles-remap-rotation'))

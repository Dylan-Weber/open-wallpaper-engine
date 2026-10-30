# particles-controlpoint-velocity: inheritcontrolpointvelocity. The object moves at +30 a second along x (a script
# on its origin), which is what moves a control point; a particle is born every second with no velocity of its own
# and lives for good, so the row of them shows what they inherited (a particle born in the first frame, while the
# control point jumps to its place, gets a huge velocity: that is why the objects sit at the origin and nothing is
# born at once). The objects sit at the scene's origin (a
# control point jumping to a place from 0 gives a particle a huge velocity). Cells: control point 1 at 0.5 and 0.25 of
# its velocity, a control point that doesn't move, the same 0.5 in a particle flagged 1, and control point 0 (which
# doesn't move).
def ramp(speed):
    def track(per_second):
        return [{"frame": 0, "value": 0, "front": {"enabled": True, "magic": True, "x": 1, "y": 0},
                 "back": {"enabled": True, "magic": True, "x": -1, "y": 0}, "lockangle": True, "locklength": True},
                {"frame": 3000, "value": per_second * 100, "front": {"enabled": True, "magic": True, "x": 1, "y": 0},
                 "back": {"enabled": True, "magic": True, "x": -1, "y": 0}, "lockangle": True, "locklength": True}]
    return {"value": "0 0 0",
            "animation": {"c0": track(speed), "c1": track(0), "c2": track(0),
                          "options": {"fps": 30, "length": 3000, "mode": "loop", "wraploop": True},
                          "relative": True}}


def cell(controlpoint, factor, flags=0, y=500):
    return {
        "material": "materials/particle/halo.json", "maxcount": 4, "starttime": 0, "flags": flags,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"}, {"id": 1, "flags": 0, "offset": "0 0 0"}],
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "100 %d 0" % y, "rate": 1,
                     "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 100000, "max": 100000},
            {"id": 3, "name": "sizerandom", "min": 30, "max": 30},
            {"id": 4, "name": "inheritcontrolpointvelocity", "controlpoint": controlpoint, "min": factor,
             "max": factor},
        ],
        "operator": [{"id": 5, "name": "movement"}],
        "renderer": [{"id": 90, "name": "sprite"}],
    }


cells = ((1, 0.5, 0, 30), (1, 0.25, 0, 30), (1, 0.5, 0, 0), (1, 0.5, 1, 30), (0, 0.5, 1, 30))
w = Wallpaper('particles-controlpoint-velocity', 'particles inheriting a control point\'s velocity', clear='0 0 0')
for k, (controlpoint, factor, flags, speed) in enumerate(cells):
    w.files['particles/c%d.json' % k] = cell(controlpoint, factor, flags, 0)
    origin = {"value": "100 %d 0" % (900 - 180 * k),
              "script": "'use strict';\nexport function update(value) {\n\tvalue.x = %d + engine.runtime * %d;\n\treturn value;\n}\n" % (100, speed)}
    w.add({"name": "cp velocity %d" % k, "particle": "particles/c%d.json" % k, "origin": origin, "scale": "1 1 1",
           "angles": "0 0 0", "visible": True})
w.save()
clock_probe.stamp(os.path.join(HERE, 'particles-controlpoint-velocity'))

# particles-maintain-drift: how fast maintaindistancebetweencontrolpoints pulls a particle at rest towards control point 0,
# for several distances between the control points. Each cell holds one particle that lives for good, half way between
# the control points (or a quarter of the way), with no movement operator but the one under test.
import math


def cell(second, place, first="0 0 0"):
    return {
        "material": "materials/particle/halo.json", "maxcount": 4, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": first}, {"id": 1, "flags": 0, "offset": second}],
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": place, "rate": 0, "instantaneous": 1,
                     "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 100000, "max": 100000},
            {"id": 3, "name": "sizerandom", "min": 30, "max": 30},
        ],
        "operator": [{"id": 6, "name": "maintaindistancebetweencontrolpoints"}],
        "renderer": [{"id": 90, "name": "sprite"}],
    }


def along(length, fraction=0.5):
    return cell("%g 0 0" % length, "%g 0 0" % (length * fraction))


d = 200 / math.sqrt(2)
cells = (
    along(100), along(150), along(200), along(250),
    along(300), along(400), along(450), along(175),
    along(400, 0.25), cell("%g %g 0" % (d, d), "%g %g 0" % (d / 2, d / 2)),
    cell("300 0 0", "200 0 0", first="100 0 0"), cell("200 0 0", "100 60 0"),
)
w = Wallpaper('particles-maintain-drift', 'maintaindistancebetweencontrolpoints drift by control point distance', clear='0 0 0')
for k, c in enumerate(cells):
    w.files['particles/c%d.json' % k] = c
    w.add({"name": "drift %d" % k, "particle": "particles/c%d.json" % k,
           "origin": "%d %d 0" % (100 + 620 * (k % 3), 900 - 200 * (k // 3)), "scale": "1 1 1", "angles": "0 0 0",
           "visible": True})
w.save()
clock_probe.stamp(os.path.join(HERE, 'particles-maintain-drift'))

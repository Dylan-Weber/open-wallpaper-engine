# particles-colorlist: the colorlist initializer, one particle per cell so the picture is exact. Cell 0: a list of
# one colour (what it paints); cell 1: the list after a yellow colorrandom; cell 2: the colorrandom after the list;
# cell 3: a list of three equal colours.
def cell(colors, count, before=None, after=None):
    initializer = [
        {"id": 2, "name": "lifetimerandom", "min": 100000, "max": 100000},
        {"id": 3, "name": "sizerandom", "min": 40, "max": 40},
    ]
    if before:
        initializer.append(before)
    initializer.append({"id": 9, "name": "colorlist", "colors": colors})
    if after:
        initializer.append(after)
    return {
        "material": "materials/particle/halo.json", "maxcount": count, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"}],
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "0 0 0", "rate": 0, "instantaneous": count,
                     "speedmin": 0, "speedmax": 0}],
        "initializer": initializer,
        "operator": [],
        "renderer": [{"id": 90, "name": "sprite"}],
    }


one = ["0.5 1 0.25"]
yellow = {"id": 5, "name": "colorrandom", "min": "255 255 0", "max": "255 255 0"}
cells = (
    (one, None, None),
    (one, yellow, None),
    (one, None, yellow),
    (one * 3, None, None),
)
w = Wallpaper('particles-colorlist', 'particle colours picked from a list', clear='0 0 0')
for k, (colors, before, after) in enumerate(cells):
    w.files['particles/c%d.json' % k] = cell(colors, 1, before, after)
    w.add({"name": "colorlist %d" % k, "particle": "particles/c%d.json" % k,
           "origin": "%d 540 0" % (250 + 400 * k), "scale": "1 1 1", "angles": "0 0 0", "visible": True})
w.save()
clock_probe.stamp(os.path.join(HERE, 'particles-colorlist'))

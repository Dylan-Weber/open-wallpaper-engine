# particles-rope-color: how the rope renderer shades a segment between particles of different colour and opacity
# (dripping_water's splash ropes: owe draws blocks of different brightness, Wallpaper Engine smooth ones). Cells:
# (A) six particles at rest from control point 0 to 1, coloured red to blue by their place (remapvalue), beam_0 texture;
# (B) the same, white, opacity 1 to 0.1; (C) a stream (5 a second, thrown up and right, falling) coloured red to blue by
# age, hose_4 texture, the splash's renderer settings (uvscale 2, uvscrolling, uvsmoothing off); (D) the same with a plain
# rope; (E) as C with the splash's operators (alphafade in 0.2, size 2 -> 0 over the life).
def rest(remap, texture):
    return {
        "material": "materials/rope_%s.json" % texture, "maxcount": 6, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "offset": "-250 0 0"}, {"id": 1, "offset": "250 0 0"}],
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "0 0 0", "rate": 0, "instantaneous": 6,
                     "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 100000, "max": 100000},
            {"id": 3, "name": "sizerandom", "min": 40, "max": 40},
            {"id": 4, "name": "mapsequencebetweencontrolpoints", "count": 6},
        ],
        "operator": [{"id": 5, "name": "movement", "drag": 0},
                     dict({"id": 6, "name": "remapvalue", "input": "positionbetweentwocontrolpoints",
                           "inputcontrolpoint0": 0, "inputcontrolpoint1": 1}, **remap)],
        "renderer": [{"id": 90, "name": "rope"}],
    }


def stream(renderer, texture='hose_4', splash=False):
    operators = [{"id": 5, "name": "movement", "drag": 0, "gravity": "0 -300 0"},
                 {"id": 6, "name": "remapvalue", "input": "lifetimefraction", "output": "color",
                  "outputrangemin": "1 0 0", "outputrangemax": "0 0 1"}]
    if splash:
        operators += [{"id": 7, "name": "alphafade", "fadeintime": 0.2},
                      {"id": 8, "name": "sizechange", "startvalue": 2, "endvalue": 0}]
    return {
        "material": "materials/rope_%s.json" % texture, "maxcount": 16, "starttime": 0, "flags": 0,
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "0 0 0", "rate": 5, "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 2, "max": 2},
            {"id": 3, "name": "sizerandom", "min": 40, "max": 40},
            {"id": 4, "name": "velocityrandom", "min": "150 250 0", "max": "150 250 0"},
        ],
        "operator": operators,
        "renderer": [dict({"id": 90, "name": "rope"}, **renderer)],
    }


splash = {"uvscale": 2, "uvscrolling": True, "uvsmoothing": False}
cells = (
    ("rest colour", rest({"output": "color", "outputrangemin": "1 0 0", "outputrangemax": "0 0 1"}, 'beam_0')),
    ("rest opacity", rest({"output": "opacity", "outputrangemin": 1, "outputrangemax": 0.1}, 'beam_0')),
    ("stream, splash settings", stream(splash)),
    ("stream, plain rope", stream({})),
    ("stream, splash operators", stream(splash, splash=True)),
)
name = 'particles-rope-color'
w = Wallpaper(name, 'rope renderer: colour and opacity along and between segments', clear='0 0 0')
for texture in ('hose_4', 'beam_0'):
    w.files['materials/rope_%s.json' % texture] = {"passes": [{
        "blending": "additive", "cullmode": "nocull", "depthtest": "disabled", "depthwrite": "disabled",
        "shader": "genericparticle", "textures": ["particle/beam/%s" % texture]}]}
places = ((480, 880), (1440, 880), (200, 420), (840, 420), (1480, 420))
for k, ((label, system), (x, y)) in enumerate(zip(cells, places)):
    w.files['particles/c%d.json' % k] = system
    w.add({"name": label, "particle": "particles/c%d.json" % k, "origin": "%d %d 0" % (x, y), "scale": "1 1 1",
           "angles": "0 0 0"})
w.save()
clock_probe.stamp(os.path.join(HERE, name))
write(name + '/compare.json', {"probe": True, "fps": 30, "shots": 10, "shot_interval_ms": 150})

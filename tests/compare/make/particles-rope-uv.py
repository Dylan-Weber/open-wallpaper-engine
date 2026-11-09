# particles-rope-uv: the rope renderer's texture settings, as dripping_water's splash uses them. Each cell is a stream
# with no randomness (one particle every 0.1 s, thrown sideways, falling under gravity, life 1.2 s), drawn as a rope
# with the hose_4 texture: plain, uvscale 2, uvscrolling, uvscale 2 with uvscrolling, uvsmoothing off, subdivision 0,
# and splash's settings (uvscale 2, uvscrolling, uvsmoothing off).
def cell(renderer):
    material = 'materials/rope_hose.json'
    return {
        "material": material, "maxcount": 16, "starttime": 0, "flags": 0,
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "0 0 0", "rate": 10, "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 1.2, "max": 1.2},
            {"id": 3, "name": "sizerandom", "min": 30, "max": 30},
            {"id": 4, "name": "velocityrandom", "min": "250 150 0", "max": "250 150 0"},
        ],
        "operator": [{"id": 5, "name": "movement", "drag": 0, "gravity": "0 -500 0"}],
        "renderer": [dict({"id": 90, "name": "rope"}, **renderer)],
    }


cells = (
    ("plain", {}),
    ("uvscale 2", {"uvscale": 2}),
    ("uvscrolling", {"uvscrolling": True}),
    ("uvscale 2, uvscrolling", {"uvscale": 2, "uvscrolling": True}),
    ("uvsmoothing off", {"uvsmoothing": False}),
    ("subdivision 0", {"subdivision": 0}),
    ("splash", {"uvscale": 2, "uvscrolling": True, "uvsmoothing": False}),
)
name = 'particles-rope-uv'
w = Wallpaper(name, 'rope renderer: uvscale, uvscrolling, uvsmoothing, subdivision', clear='0 0 0')
w.files['materials/rope_hose.json'] = {"passes": [{
    "blending": "additive", "cullmode": "nocull", "depthtest": "disabled", "depthwrite": "disabled",
    "shader": "genericparticle", "textures": ["particle/beam/hose_4"]}]}
for k, (label, renderer) in enumerate(cells):
    w.files['particles/c%d.json' % k] = cell(renderer)
    w.add({"name": label, "particle": "particles/c%d.json" % k,
           "origin": "%d %d 0" % (120 + 460 * (k % 4), 1000 - 480 * (k // 4)), "scale": "1 1 1", "angles": "0 0 0"})
w.save()
clock_probe.stamp(os.path.join(HERE, name))
write(name + '/compare.json', {"probe": True, "fps": 30, "shots": 10, "shot_interval_ms": 150})

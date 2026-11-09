# particles-rope-shape: how the rope renderer joins its particles (dripping_water's splash: Wallpaper Engine draws a few
# long pieces with kinks, owe a smooth curve). Particles at rest on a circle
# (mapsequencearoundcontrolpoint throws them outwards at 100 units/s, drag 1 stops them on a ring) are drawn as a rope: 6, 8 and 12 particles with the hose_4 texture, 8 with beam_0, 8 with subdivision 0, and 8 as sprites
# to show where the particles are.
def cell(count, renderer, texture='particle/beam/hose_4'):
    return {
        "material": "materials/rope_%s.json" % texture.split('/')[-1], "maxcount": count, "starttime": 0, "flags": 0,
        "emitter": [{"id": 1, "name": "boxrandom", "distancemin": "0 0 0", "distancemax": "0 0 0",
                     "directions": "1 1 0", "origin": "0 0 0", "rate": 0, "instantaneous": count,
                     "speedmin": 0, "speedmax": 0}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 100000, "max": 100000},
            {"id": 3, "name": "sizerandom", "min": 24, "max": 24},
            {"id": 4, "name": "mapsequencearoundcontrolpoint", "count": count, "speedmin": "100 100 0", "speedmax": "100 100 0"},
        ],
        "operator": [{"id": 5, "name": "movement", "drag": 1}],
        "renderer": [dict({"id": 90}, **renderer)],
    }


name = 'particles-rope-shape'
w = Wallpaper(name, 'rope renderer: how particles are joined', clear='0 0 0')
for texture in ('particle/beam/hose_4', 'particle/beam/beam_0'):
    w.files['materials/rope_%s.json' % texture.split('/')[-1]] = {"passes": [{
        "blending": "additive", "cullmode": "nocull", "depthtest": "disabled", "depthwrite": "disabled",
        "shader": "genericparticle", "textures": [texture]}]}
cells = (
    ("6 hose", cell(6, {"name": "rope"})),
    ("8 hose", cell(8, {"name": "rope"})),
    ("12 hose", cell(12, {"name": "rope"})),
    ("8 beam", cell(8, {"name": "rope"}, 'particle/beam/beam_0')),
    ("8 subdivision 0", cell(8, {"name": "rope", "subdivision": 0})),
    ("8 sprites", cell(8, {"name": "sprite"})),
)
for k, (label, system) in enumerate(cells):
    w.files['particles/c%d.json' % k] = system
    w.add({"name": label, "particle": "particles/c%d.json" % k,
           "origin": "%d %d 0" % (330 + 630 * (k % 3), 760 - 400 * (k // 3)), "scale": "1 1 1", "angles": "0 0 0"})
w.save()
clock_probe.stamp(os.path.join(HERE, name))
write(name + '/compare.json', {"probe": True, "fps": 30})

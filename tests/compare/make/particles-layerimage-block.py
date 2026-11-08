# particles-layerimage-block: how Wallpaper Engine reads the 4x4 texel block behind each node of the layerimage lattice
# (particles-layerimage-pitch: one particle at the centre of each 4x4 block). 32-pixel images, each 4x4 block alike:
# only the central 2x2 opaque (a box average gives 0.25, a bilinear tap at the centre 1); all but the central 2x2 (0.75 or 0);
# the left two columns (0.5 both ways); whole blocks with an opacity from 0 to 1 in eight steps along x (the threshold);
# blocks half red and half blue (the colour: purple if averaged); and red blocks with one opaque blue texel.
def particle(maxcount):
    return {
        "material": "materials/particle/halo.json", "maxcount": maxcount, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"}],
        "emitter": [{"id": 1, "name": "layerimage", "rate": 100000}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 100000, "max": 100000},
            {"id": 3, "name": "sizerandom", "min": 2, "max": 2},
        ],
        "operator": [],
        "renderer": [{"id": 90, "name": "sprite"}],
    }


def image(texel):
    return b''.join(bytes(texel(x, y)) for y in range(32) for x in range(32))


patterns = [
    ('centre', lambda x, y: (255, 255, 255, 255 if x % 4 in (1, 2) and y % 4 in (1, 2) else 0)),
    ('ring', lambda x, y: (255, 255, 255, 0 if x % 4 in (1, 2) and y % 4 in (1, 2) else 255)),
    ('left', lambda x, y: (255, 255, 255, 255 if x % 4 < 2 else 0)),
    ('ramp', lambda x, y: (255, 255, 255, round(255 * (x // 4) / 7))),
    ('halves', lambda x, y: (255, 0, 0, 255) if x % 4 < 2 else (0, 0, 255, 255)),
    ('oneblue', lambda x, y: (0, 0, 255, 255) if x % 4 == 0 and y % 4 == 0 else (255, 0, 0, 255)),
    ('rampfine', lambda x, y: (255, 255, 255, round(255 * (0.375 + 0.25 * (x // 4) / 7)))),
]
w = Wallpaper('particles-layerimage-block', 'layerimage lattice: reading a 4x4 block', clear='0 0 0', size=(480, 270))
for k, (name, texel) in enumerate(patterns):
    x, y = 60 + 90 * (k % 5), 200 - 90 * (k // 5)
    w.files['particles/c%d.json' % k] = particle(20000)
    model = w.texture_model('img%d' % k, tex_file(w, name, 32, 32, image(texel)))
    layer = w.image('img%d' % k, model, x, y, 32, 32, visible=False)
    w.add({"name": "block %d" % k, "particle": "particles/c%d.json" % k, "origin": "%d %d 0" % (x, y),
           "scale": "1 1 1", "angles": "0 0 0", "visible": True,
           "dependencies": [{"id": layer, "index": 0, "type": "emitterimage"}]})
w.save()
clock_probe.stamp(os.path.join(HERE, 'particles-layerimage-block'))
with open(os.path.join(HERE, 'particles-layerimage-block', 'compare.json'), 'w') as f:
    json.dump({"probe": True, "fps": 30}, f)

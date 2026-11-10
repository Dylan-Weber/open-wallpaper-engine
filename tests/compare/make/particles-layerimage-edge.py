# particles-layerimage-edge: where a layerimage emitter puts particles when a texture's size is not a multiple of 4 (the
# lattice of particles-layerimage-pitch is one point per whole 4x4 block). 480x270 scene (a unit is several pixels), tiny
# particles. Row 1: white images 32 wide and 1, 2, 3, 5, 6, 7, 9 high; row 2: 1, 2, 3, 5, 6, 7, 9 wide and 32 high;
# row 3: the 64x2 rainbow gradient, and white 10x10 and 13x6.
def particle(maxcount, size):
    return {
        "material": "materials/particle/halo.json", "maxcount": maxcount, "starttime": 0, "flags": 0,
        "controlpoint": [{"id": 0, "flags": 0, "offset": "0 0 0"}],
        "emitter": [{"id": 1, "name": "layerimage", "rate": 100000}],
        "initializer": [
            {"id": 2, "name": "lifetimerandom", "min": 100000, "max": 100000},
            {"id": 3, "name": "sizerandom", "min": size, "max": size},
        ],
        "operator": [],
        "renderer": [{"id": 90, "name": "sprite"}],
    }


def white(width, height):
    return bytes((255, 255, 255, 255)) * (width * height)


cells = [('h%d' % h, 32, h, 30 + 60 * k, 225) for k, h in enumerate((1, 2, 3, 5, 6, 7, 9))]
cells += [('w%d' % v, v, 32, 30 + 60 * k, 150) for k, v in enumerate((1, 2, 3, 5, 6, 7, 9))]
cells += [('s10', 10, 10, 260, 60), ('s13x6', 13, 6, 340, 60)]
w = Wallpaper('particles-layerimage-edge', 'layerimage lattice at sizes not a multiple of 4', clear='0 0 0', size=(480, 270))
for k, (name, tw, th, x, y) in enumerate(cells + [('rainbow', 64, 2, 120, 60)]):
    w.files['particles/c%d.json' % k] = particle(20000, 0.8)
    texture = 'gradient/gradient_rainbow' if name == 'rainbow' else tex_file(w, name, tw, th, white(tw, th))
    model = w.texture_model('img%d' % k, texture)
    layer = w.image('img%d' % k, model, x, y, tw, th, visible=False)
    w.add({"name": "edge %d" % k, "particle": "particles/c%d.json" % k, "origin": "%d %d 0" % (x, y),
           "scale": "1 1 1", "angles": "0 0 0", "visible": True,
           "dependencies": [{"id": layer, "index": 0, "type": "emitterimage"}]})
w.save()
clock_probe.stamp(os.path.join(HERE, 'particles-layerimage-edge'))
with open(os.path.join(HERE, 'particles-layerimage-edge', 'compare.json'), 'w') as f:
    json.dump({"probe": True, "fps": 30}, f)

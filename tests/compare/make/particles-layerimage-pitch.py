# particles-layerimage-pitch: what sets the lattice of a layerimage emitter. Wallpaper Engine puts the particles on a lattice
# of 4 scene units in a 1920 and a 512 wide scene (particles-layerimage-lattice, layerimage-big). These scenes are small so a
# unit is several screen pixels. Cells: a 32-pixel white image at scale 1; a 16-pixel one at scale 2 and a 64-pixel one at
# scale 0.5 (texels or scene units?); a 32-pixel image shown at size 64; a 40x24 image (sizes not a multiple of 4); a turned
# one; and 32-pixel images opaque only at the texels whose column and row are k modulo 4 (k = 0..3: which texel of four is read).
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


def phase(size, k):
    return b''.join(bytes((255, 255, 255, 255 if x % 4 == k and y % 4 == k else 0)) for y in range(size) for x in range(size))


def lattice_wallpaper(name, title, size, cells, particle_size, zoom=1.0):
    """cells: (texture name, width, height, pixels, layer size, scale, angle, x, y)"""
    w = Wallpaper(name, title, clear='0 0 0', size=size)
    for k, (texture, tw, th, pixels, layer_size, scale, angle, x, y) in enumerate(cells):
        w.files['particles/c%d.json' % k] = particle(20000, particle_size)
        model = w.texture_model('img%d' % k, tex_file(w, texture, tw, th, pixels))
        layer = w.image('img%d' % k, model, x, y, layer_size[0], layer_size[1], angle=angle, scale='%g %g 1' % (scale, scale),
                        visible=False)
        w.add({"name": "lattice %d" % k, "particle": "particles/c%d.json" % k, "origin": "%d %d 0" % (x, y),
               "scale": "1 1 1", "angles": "0 0 0", "visible": True,
               "dependencies": [{"id": layer, "index": 0, "type": "emitterimage"}]})
    w.save()
    if zoom != 1.0:
        path = os.path.join(HERE, name, 'scene.json')
        with open(path) as f:
            scene = json.load(f)
        scene['general']['zoom'] = zoom
        write(name + '/scene.json', scene)
    clock_probe.stamp(os.path.join(HERE, name))
    with open(os.path.join(HERE, name, 'compare.json'), 'w') as f:
        json.dump({"probe": True, "fps": 30}, f)


# 480x270: one unit is 4 pixels of a 1920 screen (3.33 at 1600)
cells = [('w32', 32, 32, white(32, 32), (32, 32), 1, 0, 60, 200),
         ('w16', 16, 16, white(16, 16), (16, 16), 2, 0, 140, 200),
         ('w64', 64, 64, white(64, 64), (64, 64), 0.5, 0, 220, 200),
         ('w32b', 32, 32, white(32, 32), (64, 64), 1, 0, 310, 200),
         ('w40x24', 40, 24, white(40, 24), (40, 24), 1, 0, 410, 200),
         ('w32c', 32, 32, white(32, 32), (32, 32), 1, 30, 60, 110)]
cells += [('phase%d' % k, 32, 32, phase(32, k), (32, 32), 1, 0, 140 + 80 * k, 110) for k in range(4)]
lattice_wallpaper('particles-layerimage-pitch', 'layerimage lattice: texels or units', (480, 270), cells, 1)

# other scene sizes and a camera zoom, a 64-pixel white image at scale 1 in the middle (and one at scale 4)
for name, size, zoom in (('particles-layerimage-pitch-1280', (1280, 720), 1.0), ('particles-layerimage-pitch-3840', (3840, 2160), 1.0),
                         ('particles-layerimage-pitch-portrait', (1080, 1920), 1.0), ('particles-layerimage-pitch-square', (1000, 1000), 1.0),
                         ('particles-layerimage-pitch-zoom', (480, 270), 2.0)):
    cx, cy = size[0] // 2, size[1] // 2
    big = size[0] / 480.0
    lattice_wallpaper(name, 'layerimage lattice, scene %dx%d zoom %g' % (size[0], size[1], zoom), size,
                      [('w64', 64, 64, white(64, 64), (64, 64), round(big), 0, cx - int(60 * big), cy),
                       ('w16', 16, 16, white(16, 16), (16, 16), round(4 * big), 0, cx + int(60 * big), cy)],
                      max(1, round(big)), zoom)

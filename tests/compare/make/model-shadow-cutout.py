# model-shadow-cutout: a ceiling whose texture is a checkerboard of opaque and transparent squares, over a lit floor and
# under a sun: does the shadow follow the squares, or is the ceiling a whole (Ocarina of Time's courtyard has grates
# and leaves)? Three ceilings: translucent, additive and normal blending.
import math
pixels = bytearray()
for y in range(8):
    for x in range(8):
        pixels += bytes((220, 220, 220, 255 if (x // 2 + y // 2) % 2 == 0 else 0))
w = Scene3D('model-shadow-cutout', 'a checkerboard of opaque and transparent squares in the sun', ambient='0.05 0.05 0.05')
lit = w.material('lit', '1 1 1', lighting=True)
w.cube('floor', lit, 0, -1.6, 6, (0, 0, 0), '8 0.1 8')
tex_file(w, 'checker', 8, 8, bytes(pixels))
for name, x, blending in (('translucent', -4.5, 'translucent'), ('additive', 0, 'additive'), ('normal', 4.5, 'normal')):
    path = 'materials/ceiling-%s.json' % name
    w.files[path] = {"passes": [{
        "shader": "generic4", "blending": blending, "cullmode": "nocull", "depthtest": "enabled",
        "depthwrite": "enabled", "combos": {"LIGHTING": 1}, "constantshadervalues": {}, "textures": ['checker']}]}
    mdl = 'models/ceiling-%s.mdl' % name
    write_binary(os.path.join(w.name, mdl), quad_mdl(path, 1.6))
    w.cube('ceiling ' + name, mdl, x, 1.2, 6, (math.pi / 2, 0, 0))
w.light('sun', 'ldirectional', 0, 6, 5, '1 1 1', 3, 30, (0.5, 0, -0.9), castshadow=True)
w.light_config = lambda: {"point": 0, "spot": 0, "tube": 0, "directional": 1, "directionalshadow": 1}
w.save()

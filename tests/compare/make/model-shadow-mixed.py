# model-shadow-mixed: one ceiling model of two meshes over a lit floor under a sun, an opaque one and then a
# translucent one: only the opaque mesh casts its shadow (Ocarina of Time's courtyard is one model of 25 materials, its
# last a translucent window, and its walls and roof shade the corridor)
import math
w = Scene3D('model-shadow-mixed', 'an opaque and a translucent part of one model in the sun', ambient='0.05 0.05 0.05')
lit = w.material('lit', '1 1 1', lighting=True)
w.cube('floor', lit, 0, -1.6, 6, (0, 0, 0), '8 0.1 8')
for name, blending in (('opaque', 'normal'), ('glass', 'translucent')):
    w.files['materials/ceiling-%s.json' % name] = {"passes": [{
        "shader": "generic4", "blending": blending, "cullmode": "nocull", "depthtest": "enabled",
        "depthwrite": "enabled", "combos": {"LIGHTING": 1}, "constantshadervalues": {"color": "1 1 1", "alpha": 0.6}, "textures": [None]}]}
write_binary(os.path.join(w.name, 'models/ceiling.mdl'),
             quads_mdl([('materials/ceiling-opaque.json', 1.8, (-2.2, 0)), ('materials/ceiling-glass.json', 1.8, (2.2, 0))]))
w.cube('ceiling', 'models/ceiling.mdl', 0, 1.2, 6, (math.pi / 2, 0, 0))
w.light('sun', 'ldirectional', 0, 6, 5, '1 1 1', 3, 30, (0.5, 0, -0.9), castshadow=True)
w.light_config = lambda: {"point": 0, "spot": 0, "tube": 0, "directional": 1, "directionalshadow": 1}
w.save()

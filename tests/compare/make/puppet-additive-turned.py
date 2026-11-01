# puppet-additive-turned: as puppet-additive, but the middle bone's own frame is turned a quarter in the bind pose (Gun Girl's
# bones have turned frames): an animation turning it 0.4 rad further, drawn as a normal layer, as an additive layer over a
# normal rest layer, and as an additive layer alone; and an additive layer turning the middle and the tip bone.
import math
w = Wallpaper('puppet-additive-turned', 'additive layers on a bone with a turned frame', clear='0 0 0')
vertices, indices = strip_mesh(bones=3)
one = (1, 1, 1)
q = math.pi / 2
def pose(mid, tip):
    return [[((0, 0, 0), (0, 0, 0), one)] * 4, [((0, 0, 0), (0, 0, mid), one)] * 4, [((0, 0, 0), (0, 0, tip), one)] * 4]
animations = [('rest', 30.0, pose(q, -q)), ('bend', 30.0, pose(q + 0.4, -q)), ('both', 30.0, pose(q + 0.4, -q + 0.3))]
variants = (('normal bend', 900, [(2, False)]), ('rest, then additive bend', 680, [(1, False), (2, True)]),
            ('additive bend alone', 460, [(2, True)]), ('rest, then additive bend of two bones', 240, [(1, False), (3, True)]))
for i, (title, y, layers) in enumerate(variants):
    write_binary('puppet-additive-turned/models/strip%d_puppet.mdl' % i, puppet_mdl(
        'materials/strip.json', vertices, indices,
        [('root', None, (0, 0, 0)), ('mid', 0, (150, 0, 0, q)), ('tip', 1, (0, -150, 0, -q))], animations))
    model = w.texture_model('strip%d' % i, 'gradient/gradient_rainbow',
                            model_extra={"puppet": "models/strip%d_puppet.mdl" % i})
    w.add({"name": "strip %d" % i, "image": model, "origin": "760 %d 0" % y, "size": "600 100", "color": "1 1 1",
           "alpha": 1, "angles": "0 0 0", "scale": "1 1 1", "alignment": "center",
           "animationlayers": [{"animation": a, "additive": add, "blend": 1.0, "blendin": False, "blendout": False,
                                "blendtime": 0.5, "rate": 1.0, "visible": True, "id": 100 + k, "name": "layer %d" % k}
                               for k, (a, add) in enumerate(layers)]})
w.save()
with open(os.path.join(HERE, 'puppet-additive-turned', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'puppet-additive-turned'))

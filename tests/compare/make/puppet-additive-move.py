# puppet-additive-move: additive animation layers that move a bone (not only turn it): the middle bone moved 40 along y, as a
# normal layer and as an additive layer over a rest layer, with the root's frame turned by 0 and by 90 degrees, and an
# additive move together with a turn. Gun Girl's additive Blink and Mood layers place her 1 to 3 pixels off in owe.
import math
w = Wallpaper('puppet-additive-move', 'additive layers that move bones', clear='0 0 0')
vertices, indices = strip_mesh(bones=3)
one = (1, 1, 1)
def pose(q, mid_pos=(0, 0, 0), mid_turn=0.0):
    return [[((0, 0, 0), (0, 0, q), one)] * 4, [(mid_pos, (0, 0, mid_turn), one)] * 4, [((0, 0, 0), (0, 0, 0), one)] * 4]
variants = []
for i, (title, q, layers) in enumerate((('normal move, root 0', 0, [(2, False)]), ('rest + additive move, root 0', 0, [(1, False), (2, True)]),
                                        ('normal move, root 90', math.pi / 2, [(2, False)]), ('rest + additive move, root 90', math.pi / 2, [(1, False), (2, True)]),
                                        ('rest + additive move and turn, root 90', math.pi / 2, [(1, False), (3, True)]))):
    animations = [('rest', 30.0, pose(q)), ('move', 30.0, pose(q, (0, 40, 0))), ('both', 30.0, pose(q, (0, 40, 0), 0.3))]
    c, s = math.cos(q), math.sin(q)
    step = (150 * c, -150 * s, 0) if q else (150, 0, 0)
    write_binary('puppet-additive-move/models/strip%d_puppet.mdl' % i, puppet_mdl(
        'materials/strip.json', vertices, indices,
        [('root', None, (0, 0, 0, q)), ('mid', 0, (150, 0, 0)), ('tip', 1, (150, 0, 0))], animations))
    model = w.texture_model('strip%d' % i, 'gradient/gradient_rainbow',
                            model_extra={"puppet": "models/strip%d_puppet.mdl" % i})
    w.add({"name": "strip %d" % i, "image": model, "origin": "%d %d 0" % (500 + (i % 2) * 900, 880 - (i // 2) * 300),
           "size": "600 100", "color": "1 1 1", "alpha": 1, "angles": "0 0 0", "scale": "1 1 1", "alignment": "center",
           "animationlayers": [{"animation": a, "additive": add, "blend": 1.0, "blendin": False, "blendout": False,
                                "blendtime": 0.5, "rate": 1.0, "visible": True, "id": 100 + k, "name": "layer %d" % k}
                               for k, (a, add) in enumerate(layers)]})
w.save()
with open(os.path.join(HERE, 'puppet-additive-move', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'puppet-additive-move'))

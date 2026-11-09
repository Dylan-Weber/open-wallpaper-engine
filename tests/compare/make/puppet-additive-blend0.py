# puppet-additive-blend0: strips with additive animation layers at blend 0, which should add nothing. Gun Girl (psim) moves a
# pixel or two in Wallpaper Engine when her additive Blink and Mood layers are there at blend 0, and owe leaves her as without
# them. The layers' animations: the rest pose; a constant bend of the middle bone (0.4 rad); the rest pose first, then the
# bend; and a scale of 1.2 on the middle bone. A strip without layers is the control.
w = Wallpaper('puppet-additive-blend0', 'additive layers at blend 0', clear='0 0 0')
vertices, indices = strip_mesh(bones=3)
one = (1, 1, 1)
still = [((0, 0, 0), (0, 0, 0), one)] * 4
bend = [((0, 0, 0), (0, 0, 0.4), one)] * 4
later = [((0, 0, 0), (0, 0, 0), one)] + [((0, 0, 0), (0, 0, 0.4), one)] * 3
grown = [((0, 0, 0), (0, 0, 0), (1.2, 1.2, 1.2))] * 4
animations = [('rest', 30.0, [still, still, still]), ('bend', 30.0, [still, bend, still]),
              ('later', 30.0, [still, later, still]), ('grown', 30.0, [still, grown, still])]
variants = (('no layers', []), ('rest', [1]), ('bend', [2]), ('later', [3]), ('grown', [4]))
for i, (title, layers) in enumerate(variants):
    write_binary('puppet-additive-blend0/models/strip%d_puppet.mdl' % i, puppet_mdl(
        'materials/strip.json', vertices, indices,
        [('root', None, (0, 0, 0)), ('mid', 0, (150, 0, 0)), ('tip', 1, (150, 0, 0))], animations))
    model = w.texture_model('strip%d' % i, 'gradient/gradient_rainbow',
                            model_extra={"puppet": "models/strip%d_puppet.mdl" % i})
    w.add({"name": "strip %d %s" % (i, title), "image": model,
           "origin": "%d %d 0" % (560 if i % 2 == 0 else 1260, 920 - (i // 2) * 250), "size": "450 75",
           "color": "1 1 1", "alpha": 1, "angles": "0 0 0", "scale": "1 1 1", "alignment": "center",
           "animationlayers": [{"animation": a, "additive": True, "blend": 0.0, "blendin": False, "blendout": False,
                                "blendtime": 0.5, "rate": 1.0, "visible": True, "id": 100 + k, "name": "layer %d" % k}
                               for k, a in enumerate(layers)]})
w.save()
with open(os.path.join(HERE, 'puppet-additive-blend0', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'puppet-additive-blend0'))

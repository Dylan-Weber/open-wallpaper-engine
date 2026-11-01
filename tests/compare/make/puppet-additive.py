# puppet-additive: strips posed by animation layers, to see what an additive layer adds to: a layer bending the middle bone
# by 0.4 rad (every frame) drawn as a normal layer, as an additive layer alone, as an additive layer over a normal one that
# poses nothing, and an additive layer whose animation starts at 0.2 rad and holds 0.4 (if Wallpaper Engine adds the change
# from the animation's first frame, that strip bends half as much). Gun Girl's additive Blink and Mood layers move her a
# pixel or two in owe and not in Wallpaper Engine.
w = Wallpaper('puppet-additive', 'what an additive animation layer adds to', clear='0 0 0')
vertices, indices = strip_mesh(bones=3)
one = (1, 1, 1)
still = [((0, 0, 0), (0, 0, 0), one)] * 4
bend = [((0, 0, 0), (0, 0, 0.4), one)] * 4
ramp = [((0, 0, 0), (0, 0, 0.2), one)] + [((0, 0, 0), (0, 0, 0.4), one)] * 3
animations = [('rest', 30.0, [still, still, still]), ('bend', 30.0, [still, bend, still]),
              ('ramp', 30.0, [still, ramp, still])]
variants = (('normal bend', 900, [(2, False)]), ('additive bend', 680, [(2, True)]),
            ('rest, then additive bend', 460, [(1, False), (2, True)]), ('additive ramp', 240, [(3, True)]))
for i, (title, y, layers) in enumerate(variants):
    write_binary('puppet-additive/models/strip%d_puppet.mdl' % i, puppet_mdl(
        'materials/strip.json', vertices, indices,
        [('root', None, (0, 0, 0)), ('mid', 0, (150, 0, 0)), ('tip', 1, (150, 0, 0))], animations))
    model = w.texture_model('strip%d' % i, 'gradient/gradient_rainbow',
                            model_extra={"puppet": "models/strip%d_puppet.mdl" % i})
    w.add({"name": "strip %d" % i, "image": model, "origin": "760 %d 0" % y, "size": "600 100", "color": "1 1 1",
           "alpha": 1, "angles": "0 0 0", "scale": "1 1 1", "alignment": "center",
           "animationlayers": [{"animation": a, "additive": add, "blend": 1.0, "blendin": False, "blendout": False,
                                "blendtime": 0.5, "rate": 1.0, "visible": True, "id": 100 + k, "name": "layer %d" % k}
                               for k, (a, add) in enumerate(layers)]})
w.save()
with open(os.path.join(HERE, 'puppet-additive', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'puppet-additive'))

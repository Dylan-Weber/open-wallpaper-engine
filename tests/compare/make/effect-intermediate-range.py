# effect-intermediate-range: does an effect's result keep values above 1 for the next effect? Two tints with the
# multiply blend (BLENDMODE 2) chained on a picture of clouds: x4 then x0.25 (a picture that keeps its range comes
# back as it was; one clipped between the effects comes out flat at 0.25 wherever it was above 0.25), and x2 then x0.5.
# Plain picture first, then the single x0.25 tint, for comparison.
SHIPPED_PASSES['tint'] = 1


def two(a, b, id):
    first = shipped('tint', {"color": "%g %g %g" % (a, a, a), "alpha": 1}, {"BLENDMODE": 2}, id=id)
    second = shipped('tint', {"color": "%g %g %g" % (b, b, b), "alpha": 1}, {"BLENDMODE": 2}, id=id + 1)
    first['passes'] = first['passes'] + second['passes']
    return first


cells = [('plain', {"passes": [], "file": "", "name": "", "visible": False}),
         ('x0.25', shipped('tint', {"color": "0.25 0.25 0.25", "alpha": 1}, {"BLENDMODE": 2})),
         ('x4 then x0.25', None), ('x2 then x0.5', None)]
w = Wallpaper('effect-intermediate-range', 'values above 1 between two effects', clear='0.12 0.12 0.12')
model = w.texture_model('picture', 'util/clouds_256')
for i, (label, instance) in enumerate(cells):
    x = 260 + (i % 4) * 460
    y = 880
    if label == 'plain':
        effects = []
    elif label == 'x0.25':
        effects = [dict(instance, id=100 + i * 10, passes=[dict(instance['passes'][0], id=1000 + i * 10)])]
    else:
        a, b = (4, 0.25) if label.startswith('x4') else (2, 0.5)
        effects = [{"file": "effects/tint/effect.json", "id": 100 + i * 10, "name": "", "visible": True,
                    "passes": [{"id": 1000 + i * 10, "constantshadervalues": {"color": "%g %g %g" % (a, a, a),
                                                                               "alpha": 1},
                                "combos": {"BLENDMODE": 2}}]},
                   {"file": "effects/tint/effect.json", "id": 101 + i * 10, "name": "", "visible": True,
                    "passes": [{"id": 1001 + i * 10, "constantshadervalues": {"color": "%g %g %g" % (b, b, b),
                                                                               "alpha": 1},
                                "combos": {"BLENDMODE": 2}}]}]
    w.image(label, model, x, y, 256, 256, scale='1.1 1.1 1', effects=effects)
w.save()
copy_shipped_effects(w, ['tint'])
with open(os.path.join(HERE, 'effect-intermediate-range', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'effect-intermediate-range'))

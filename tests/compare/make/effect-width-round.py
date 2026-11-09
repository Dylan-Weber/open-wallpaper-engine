# effect-width-round: a picture drawn through an effect (a tint that keeps it as it is) at widths that are and aren't
# multiples of 16, beside the same pictures without the effect. Gun Girl (psim, a 3800x3000 puppet layer with two tints) is
# 0.22 % wider in Wallpaper Engine than in owe, about 3808 / 3800: does Wallpaper Engine round the effect's buffer up to a
# multiple of 16 and stretch the picture over it?
w = Wallpaper('effect-width-round', 'effect buffers at widths not a multiple of 16', clear='0 0 0')
model = w.texture_model('picture', 'gradient/gradient_rainbow')
same = shipped('tint', {"color": "1 1 1", "alpha": 0}, id=10)
cells = [(1000, None), (1000, same), (1001, same), (1008, same), (1009, same), (1016, None)]
for i, (width, effect) in enumerate(cells):
    instance = None
    if effect:
        instance = dict(effect, id=100 + i * 10)
        instance['passes'] = [dict(p, id=1000 + i * 10 + n) for n, p in enumerate(effect['passes'])]
    w.image('width %d %s' % (width, 'tint' if effect else 'plain'), model, 960, 980 - i * 150, width, 100,
            **({"effects": [instance]} if instance else {}))
w.save()
copy_shipped_effects(w, ['tint'])
with open(os.path.join(HERE, 'effect-width-round', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'effect-width-round'))

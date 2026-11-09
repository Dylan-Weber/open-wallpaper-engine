# animation-color-relative: which colour animations Wallpaper Engine adds to the colour and which replace it (OoT's ocarina
# buttons replace, #19's probes add). Squares of colour 0 0.5 0 with a constant animation of 0 0 0.5 (add: teal 0 0.5 0.5,
# replace: dark blue 0 0 0.5): without a "relative" key, relative false, relative true, each without and with a name.
w = Wallpaper('animation-color-relative', 'colour animations: added or in place of the colour', clear='0.1 0.1 0.15')
squares = []
for named in (False, True):
    for relative in (None, False, True):
        a = constant_animation([0, 0, 0.5])
        if relative is None:
            del a['relative']
        else:
            a['relative'] = relative
        if named:
            a['options'] = dict(a['options'], name='fade')
        squares.append(('%s%s' % ({None: 'no key', False: 'relative false', True: 'relative true'}[relative],
                                   ', named' if named else ''), a))
for i, (label, animation) in enumerate(squares):
    w.solid(label, 250 + (i % 3) * 700, 760 - (i // 3) * 450, 300, 300, color={"value": "0 0.5 0", "animation": animation})
w.save()
with open(os.path.join(HERE, w.name, 'compare.json'), 'w') as f:
    json.dump({"probe": True, "fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, w.name))

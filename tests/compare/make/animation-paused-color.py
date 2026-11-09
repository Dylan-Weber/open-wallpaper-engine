# animation-paused-color: OoT's ocarina buttons (skinned-3737268876) have a colour animation "fade" (1 -> 0.41 -> 0.41 -> 1
# over 6 frames, mode single, startpaused) that only a click plays; Wallpaper Engine draws them ~0.73 as bright as owe, which
# adds a colour animation to the colour (#19). Squares of colour 0 0.5 0 with a constant animation of 0 0 0.5 (add: teal,
# replace: dark blue), single or loop, playing or startpaused; then OoT's own curve (first key 1) paused and playing, on a
# colour of 0 0.5 0.
ANIM = json.load(open(os.path.join(MAKE, 'animation-paused-color.anim.json')))
w = Wallpaper('animation-paused-color', 'colour animation that starts paused', clear='0.1 0.1 0.15')
squares = []
for mode in ('single', 'loop'):
    for paused in (False, True):
        a = constant_animation([0, 0, 0.5])
        a['options'] = dict(a['options'], mode=mode, startpaused=paused)
        squares.append(('%s%s' % (mode, ' paused' if paused else ''), a))
squares.append(('OoT fade paused', ANIM))
squares.append(('OoT fade playing', dict(ANIM, options=dict(ANIM['options'], startpaused=False, mode='loop'))))
for i, (label, animation) in enumerate(squares):
    w.solid(label, 250 + (i % 3) * 700, 760 - (i // 3) * 450, 300, 300, color={"value": "0 0.5 0", "animation": animation})
w.save()
with open(os.path.join(HERE, w.name, 'compare.json'), 'w') as f:
    json.dump({"probe": True, "fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, w.name))

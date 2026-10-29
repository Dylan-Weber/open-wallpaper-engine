# puppet-physics-sway: the root bone of a strip of three bones sways 100 units to either side (an animation, 60 frames a
# swing), carrying a middle bone that turns on a spring (inertia ri 30) and a tip that moves on a spring (inertia ti 30):
# they trail and swing behind it, which is what their inertia and springs do together (a ponytail). A series of pictures one
# after another (compare.json "shots" and "fps": 30), each compared at its own frame.
w = Wallpaper('puppet-physics-sway', 'a swaying parent carrying bones on springs', clear='0 0 0')
vertices, indices = strip_mesh(bones=3)
one = (1, 1, 1)
frames = 61
sway = [[((100 * math.sin(2 * math.pi * k / 60), 0, 0), (0, 0, 0), one) for k in range(frames)],
        [((0, 0, 0), (0, 0, 0), one)] * frames,
        [((0, 0, 0), (0, 0, 0), one)] * frames]
physics = {"a": "1 0 0", "gd": "0 -1 0", "ge": False, "ik": False, "ikce": False, "ikd": 2, "ikg": True, "ikr": True,
           "ikrd": 25, "ikrmax": 3.1415927, "ikrmin": 0, "la": False, "lamax": "3.14159 3.14159 3.14159",
           "lamin": "-3.14159 -3.14159 -3.14159", "layout": "advanced", "lt": False, "ltmax": 100, "m": 20,
           "preset": "none", "presetresponsiveness": 100, "r": False, "rax": True, "ray": True, "raz": True,
           "re": False, "rf": 20, "ri": 30, "rs": 200, "s": 0, "se": True, "t": False, "tax": True, "tay": True,
           "taz": True, "tf": 20, "ti": 30, "tm": 60.0, "tp": "62.5 0 0", "ts": 200}
turn = json.dumps({**physics, "r": True})
move = json.dumps({**physics, "t": True})
write_binary('puppet-physics-sway/models/strip_puppet.mdl', puppet_mdl(
    'materials/strip.json', vertices, indices,
    [('root', None, (0, 0, 0)), ('mid', 0, (150, 0, 0), turn), ('tip', 1, (150, 0, 0), move)],
    [('sway', 30.0, sway)]))
model = w.texture_model('strip', 'gradient/gradient_rainbow', model_extra={"puppet": "models/strip_puppet.mdl"})
w.add({"name": "strip", "image": model, "origin": "960 540 0", "size": "600 100", "color": "1 1 1", "alpha": 1,
       "angles": "0 0 0", "scale": "1 1 1", "alignment": "center",
       "animationlayers": [{"animation": 1, "blend": 1.0, "rate": 1.0, "visible": True, "id": 1, "name": "sway"}]})
w.save()
with open(os.path.join(HERE, 'puppet-physics-sway', 'compare.json'), 'w') as f:
    json.dump({"shots": 140, "shot_interval_ms": 0, "fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'puppet-physics-sway'))

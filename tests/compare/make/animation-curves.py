# animation-curves: how Wallpaper Engine interpolates keyframes: squares whose x is animated from 300 to 1600 over 3
# seconds (90 frames), looping, with flat handles, handles turned off, "magic" handles, steep custom handles, three keys,
# mirror mode and single mode. Each row's x at the probed moment shows the curve there.
w = Wallpaper('animation-curves', 'keyframe curves and their handles', clear='0.05 0.05 0.1')


def key(frame, value, back=(-1, 0), front=(1, 0), enabled=True, magic=None):
    k = {"frame": frame, "value": value,
         "back": {"enabled": enabled, "x": back[0], "y": back[1]},
         "front": {"enabled": enabled, "x": front[0], "y": front[1]}}
    if magic is not None:
        k["back"]["magic"] = magic
        k["front"]["magic"] = magic
    return k


def curve(keys, mode='loop', length=90):
    return {"c0": keys, "c1": [key(0, 0)], "c2": [key(0, 0)],
            "options": {"fps": 30, "length": length, "mode": mode, "wraploop": None}, "relative": False}


rows = (("flat handles", curve([key(0, 300), key(90, 1600)])),
        ("handles off", curve([key(0, 300, enabled=False), key(90, 1600, enabled=False)])),
        ("magic handles", curve([key(0, 300, magic=True), key(90, 1600, magic=True)])),
        ("steep handles", curve([key(0, 300, (-10, -300), (10, 300)), key(90, 1600, (-10, -300), (10, 300))])),
        ("three keys", curve([key(0, 300), key(30, 1400), key(90, 1600)])),
        ("mirror", curve([key(0, 300), key(90, 1600)], mode='mirror')),
        ("single", curve([key(0, 300), key(90, 1600)], mode='single')))
for k, (name, animation) in enumerate(rows):
    y = 960 - 120 * k
    w.add({"name": name, "image": "models/util/solidlayer.json", "size": "60 60", "color": "1 0.8 0.3", "alpha": 1,
           "origin": {"value": "300 %d 0" % y, "animation": animation}, "angles": "0 0 0", "scale": "1 1 1",
           "alignment": "center", "visible": True})
    w.add({"name": name + " mark", "image": "models/util/solidlayer.json", "size": "1300 2", "color": "0.3 0.3 0.6",
           "alpha": 1, "origin": "950 %d 0" % (y - 40), "angles": "0 0 0", "scale": "1 1 1", "alignment": "center",
           "visible": True})
w.save()
with open(os.path.join(HERE, 'animation-curves', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'animation-curves'))


# animation-handles: handles as wallpapers author them (x a fraction, y in the value's units), on the same 300 -> 1600
# loop: animation-curves' steep handles (x 10) landed far apart in the two engines.
w = Wallpaper('animation-handles', 'keyframe handles as wallpapers author them', clear='0.05 0.05 0.1')
rows = (("x 0.25 y 0", (-0.25, 0), (0.25, 0)), ("x 0.5 y 0", (-0.5, 0), (0.5, 0)), ("x 1 y 0", (-1, 0), (1, 0)),
        ("x 0.25 y 300", (-0.25, -300), (0.25, 300)), ("x 0.5 y 300", (-0.5, -300), (0.5, 300)),
        ("x 1 y 300", (-1, -300), (1, 300)), ("x 0.5 y -300", (-0.5, 300), (0.5, -300)))
for k, (name, back, front) in enumerate(rows):
    y = 960 - 120 * k
    animation = curve([key(0, 300, back, front), key(90, 1600, back, front)])
    w.add({"name": name, "image": "models/util/solidlayer.json", "size": "60 60", "color": "1 0.8 0.3", "alpha": 1,
           "origin": {"value": "0 %d 0" % y, "animation": animation}, "angles": "0 0 0", "scale": "1 1 1",
           "alignment": "center", "visible": True})
w.save()
with open(os.path.join(HERE, 'animation-handles', 'compare.json'), 'w') as f:
    json.dump({"fps": 30}, f)
clock_probe.stamp(os.path.join(HERE, 'animation-handles'))

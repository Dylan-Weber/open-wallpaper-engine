# light-linked-animation: a light whose angles and colour animate together (Ocarina of Time's sun: the angles' animation
# "change" starts paused and holds the colour's as its child; the colour's own options only name the angles as its
# parent). Paused, the light stays white and unturned, so the cubes keep their lit faces.
def key(frame, value):
    return {"frame": frame, "value": value, "lockangle": True, "locklength": True,
            "back": {"enabled": True, "magic": True, "x": -1 if frame == 0 else -0.5, "y": 0},
            "front": {"enabled": True, "magic": True, "x": 0.5 if frame == 0 else 1, "y": 0}}


turn = {"c0": [key(0, 0), key(90, 0)], "c1": [key(0, 0), key(90, 2.5)], "c2": [key(0, 0), key(90, 0)],
        "options": {"children": [{"key": "color"}], "events": [{"frame": 0, "name": "pause_off"}],
                    "fps": 30, "length": 90, "mode": "mirror", "name": "change", "smoothing": None,
                    "startpaused": True, "wraploop": None},
        "relative": True}
fade = {"c%d" % i: [key(0, 1), key(90, 0)] for i in range(3)}
fade["options"] = {"fps": 30, "length": 90, "mode": "mirror", "parent": {"key": "angles"}, "smoothing": None,
                   "wraploop": None}
lit_row('light-linked-animation', 'angles and colour animations linked, the first paused', lambda w: w.light(
    'sun', 'ldirectional', 0, 0, 5, {"animation": fade, "value": "1 1 1"}, 3, 30,
    angles={"animation": turn, "value": "0 1.5708 0"}))

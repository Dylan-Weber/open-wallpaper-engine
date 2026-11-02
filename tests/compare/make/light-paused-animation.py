# light-paused-animation: a light whose colour has an animation that starts paused (Ocarina of Time's sun has one: white
# at frame 0, dark at frame 90, mirrored, "startpaused", and its script pauses it at the marker events). A paused
# animation holds its first frame, so the cubes stay lit white (owe #71).
def key(frame, value):
    return {"frame": frame, "value": value, "lockangle": True, "locklength": True,
            "back": {"enabled": True, "magic": True, "x": -1 if frame == 0 else -0.5, "y": 0},
            "front": {"enabled": True, "magic": True, "x": 0.5 if frame == 0 else 1, "y": 0}}


animation = {"c%d" % i: [key(0, 1), key(90, 0)] for i in range(3)}
animation["options"] = {"fps": 30, "length": 90, "mode": "mirror", "name": "change", "startpaused": True,
                        "smoothing": None, "wraploop": None}
lit_row('light-paused-animation', 'a light colour animation that starts paused', lambda w: w.light(
    'sun', 'ldirectional', 0, 0, 5, {"animation": animation, "value": "1 1 1"}, 3, 30, (0, 1.5708, 0)))

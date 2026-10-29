# 19f. scene-wide: a scene wider than the screen (2520 x 1080 like Wallpaper Engine's "retro"): by default it covers
# the screen and its sides are cut off; squares at the corners and the middle show what is kept
w = Wallpaper('scene-wide', 'a scene wider than the screen', clear='0.2 0.2 0.25', size=(2520, 1080))
for name, x, y, color in (('left top', 100, 980, '1 0 0'), ('right top', 2420, 980, '0 1 0'),
                          ('left bottom', 100, 100, '0 0 1'), ('right bottom', 2420, 100, '1 1 0'),
                          ('middle', 1260, 540, '1 1 1'), ('kept left', 400, 540, '1 0 1'), ('kept right', 2120, 540, '0 1 1')):
    w.solid(name, x, y, 160, 160, color)
w.save()

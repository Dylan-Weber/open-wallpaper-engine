# 9. scene-background: a bright clear colour under translucent layers
w = Wallpaper('scene-background', 'clear colour', clear='0.8 0.5 0.2')
for i, alpha in enumerate([1.0, 0.75, 0.5, 0.25]):
    w.solid('white %g' % alpha, 300 + i * 400, 750, 300, 200, '1 1 1', alpha=alpha)
    w.solid('black %g' % alpha, 300 + i * 400, 450, 300, 200, '0 0 0', alpha=alpha)
    w.solid('blue %g' % alpha, 300 + i * 400, 150, 300, 200, '0 0 1', alpha=alpha)
w.save()

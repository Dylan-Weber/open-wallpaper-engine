# 19c. how translucent models are drawn: two translucent cubes that overlap (the near one listed first) before an
# opaque one listed last that is behind both
for name, sorting in (('model-translucent-order', False),):
    w = Scene3D(name, 'two translucent models' + (', translucent sorting on' if sorting else ''), ambient='1 1 1')
    if sorting:
        w.general['transparentsorting'] = True
    red = w.material('red_glass', '1 0 0', blending='translucent', alpha=0.5)
    green = w.material('green_glass', '0 1 0', blending='translucent', alpha=0.5)
    blue = w.material('blue', '0 0 1')
    w.cube('near red', red, -1, 0, 2, (0.3, 0.5, 0), '1.5 1.5 1.5')
    w.cube('far green', green, 0.8, 0.5, -1, (0.3, -0.5, 0), '1.5 1.5 1.5')
    w.cube('behind blue', blue, 0.2, -0.3, -4, (0, 0.3, 0), '1.5 1.5 1.5')
    w.save()

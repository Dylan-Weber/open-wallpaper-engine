# model-sort-nodepth: a model whose material doesn't test depth but writes it (blending alphatocoverage), as More
# Physics' sun is, among opaque ones, listed after a near red cube and before a far green one. The sun is a big white
# cube far behind both. If it draws in scene order it paints over the red cube; if the engine draws it first (far to
# near) the red cube stays. With the scene's transparent sorting on and off.
for name, sorting in (('model-sort-nodepth', True), ('model-sort-nodepth-off', False)):
    w = Scene3D(name, 'a depth-less model among opaque ones' + (', transparent sorting on' if sorting else ''),
                ambient='1 1 1')
    if sorting:
        w.general['transparentsorting'] = True
    red = w.material('red', '1 0 0')
    sun = w.material('sun', '1 1 1', blending='alphatocoverage')
    green = w.material('green', '0 1 0')
    # no depth test, but it writes depth
    w.files['materials/sun.json']['passes'][0]['depthtest'] = 'disabled'
    w.files['materials/sun.json']['passes'][0]['depthwrite'] = 'enabled'
    w.cube('near red', red, -1.2, 0, 2, (0.3, 0.5, 0), '1.5 1.5 1.5')
    w.cube('sun', sun, 1.5, 0, -8, (0, 0.3, 0), '3 3 3')
    w.cube('far green', green, 0.5, -0.5, -4, (0.2, 0.4, 0), '1.5 1.5 1.5')
    w.save()

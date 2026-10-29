# 22b. model-light-tube: a tube light in front of a wall. The tube runs from the light's origin 2 units along its x
# axis, so scale 4 makes it 8 units long (found with experiments in the real engine)
w = Scene3D('model-light-tube', 'tube light', ambient='0.05 0.05 0.05')
lit = w.material('lit', '1 1 1', lighting=True)
w.cube('wall', lit, 0, 0, -1, (0, 0, 0), '12 7 0.2')
w.light('light', 'ltube', 0, 0, 3, '1 1 1', 6, 20, scale='4 1 1')
w.save()

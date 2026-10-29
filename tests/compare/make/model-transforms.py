# 18. model-transforms: position, rotation (radians, x then y then z as the engine applies them) and scale of models
w = Scene3D('model-transforms', 'model transforms', ambient='1 1 1')
colors = ['1 0 0', '0 1 0', '0 0 1', '1 1 0', '1 0 1', '0 1 1']
for i, (angles, scale) in enumerate([((0, 0, 0), '1 1 1'), ((0.6, 0, 0), '1 1 1'), ((0, 0.6, 0), '1 1 1'),
                                      ((0, 0, 0.6), '1 1 1'), ((0.4, 0.4, 0.4), '1 1 1'), ((0, 0, 0), '1.5 0.5 1')]):
    mdl = w.material('cube%d' % i, colors[i])
    w.cube('cube %d' % i, mdl, -4 + (i % 3) * 4, 1.8 - (i // 3) * 3.6, 0, angles, scale)
w.save()

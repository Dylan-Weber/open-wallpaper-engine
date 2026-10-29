# 22. model-light-directional: a directional light on a row of lit cubes
lit_row('model-light-directional', 'directional light', lambda w: w.light(
    'directional', 'ldirectional', 0, 0, 5, '1 1 1', 3, 30, (0, 1.5708, 0)))

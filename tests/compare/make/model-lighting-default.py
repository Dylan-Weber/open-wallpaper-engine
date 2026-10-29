# 22e2. model-lighting-default: generic4 lights a model unless its material turns LIGHTING off; left out, LIGHTING is
# 1 (and RIMLIGHTING needs it). Left to right: no combos, RIMLIGHTING only, LIGHTING 1, LIGHTING 1 and RIMLIGHTING, LIGHTING 0
w = Scene3D('model-lighting-default', 'generic4 lighting left out of the material', ambient='0.1 0.1 0.1')
for col, (label, combos) in enumerate((('none', {}), ('rim', {"RIMLIGHTING": 1}), ('lit', {"LIGHTING": 1}),
                                       ('lit rim', {"LIGHTING": 1, "RIMLIGHTING": 1}), ('unlit', {"LIGHTING": 0}))):
    w.cube(label, w.material('cube-%d' % col, '1 1 1', combos=combos), -6 + col * 3, 0, 0, angles=(20, 30, 0))
w.light('light', 'lpoint', 3, 3, 4, intensity=3, radius=30)
w.save()

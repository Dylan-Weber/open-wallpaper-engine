# 22f. model-shadow-*: a cube in front of a lit wall, its light off to one side casting a shadow on the wall (the light
# config counts one shadowed light of that kind)
shadow_scene('model-shadow-spot', 'spot light shadow', lambda w: w.light(
    'spot', 'lspot', 2, 1, 7, '1 1 1', 6, 40, (0, 1.5708, 0), innercone=20, outercone=40, castshadow=True),
    {"spot": 1, "spotshadow": 1})
shadow_scene('model-shadow-point', 'point light shadow', lambda w: w.light(
    'point', 'lpoint', 2, 1, 4, '1 1 1', 6, 30, castshadow=True), {"point": 1, "pointshadow": 1})
shadow_scene('model-shadow-directional', 'directional light shadow', lambda w: w.light(
    'directional', 'ldirectional', 0, 0, 5, '1 1 1', 3, 30, (0.3, 1.2, 0), castshadow=True),
    {"directional": 1, "directionalshadow": 1})

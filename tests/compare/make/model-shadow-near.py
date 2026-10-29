# model-shadow-near: a corridor under a sun with three shadow cascades (3, 14.34, 100): a floor and a ceiling with a gap,
# all within the first cascade, so the sun's light comes through the gap onto the floor in a patch (Ocarina of Time's
# corridor has such a patch on its floor and owe has none)
w = Scene3D('model-shadow-near', 'sun light through a gap, near the camera', ambient='0.05 0.05 0.05')
lit = w.material('lit', '1 1 1', lighting=True)
w.cube('floor', lit, 0, -1.6, 7, (0, 0, 0), '6 0.1 6')
w.cube('ceiling far', lit, 0, 1.2, 4, (0, 0, 0), '6 0.1 3')
w.cube('ceiling near', lit, 0, 1.2, 11, (0, 0, 0), '6 0.1 2')
w.light('sun', 'ldirectional', 0, 6, 5, '1 1 1', 3, 30, (0.5, 0, -0.9), castshadow=True, cascadedistance0=3.0,
        cascadedistance1=14.34, cascadedistance2=100.0)
w.light_config = lambda: {"point": 0, "spot": 0, "tube": 0, "directional": 1, "directionalshadow": 1}
w.save()

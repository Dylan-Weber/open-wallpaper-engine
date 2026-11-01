# model-shadow-cascades: a sun with three shadow cascades (3, 14.34 and 100 units, as Ocarina of Time's) over a long
# floor with cubes at 4, 18, 36 and 62 units from the camera, each casting its own shadow; the near cascades have the
# near shadows in sharp detail, and none of them may leave a floor in shadow where no cube is
w = Scene3D('model-shadow-cascades', 'directional light shadow over several cascades', ambient='0.05 0.05 0.05')
lit = w.material('lit', '1 1 1', lighting=True)
w.cube('floor', lit, 0, -1.6, -25, (0, 0, 0), '25 0.2 55')
for i, (z, size) in enumerate(((8, 0.8), (-6, 1.5), (-24, 3), (-50, 6))):
    w.cube('cube %d' % i, lit, (-1) ** i * 2.5, -1.4 + size, z, (0, 0.4, 0), '%g %g %g' % (size, size, size))
w.light('sun', 'ldirectional', 0, 6, 5, '1 1 1', 3, 30, (0.5, 0, -0.9), castshadow=True, cascadedistance0=3.0,
        cascadedistance1=14.34, cascadedistance2=100.0)
w.light_config = lambda: {"point": 0, "spot": 0, "tube": 0, "directional": 1, "directionalshadow": 1}
w.save()

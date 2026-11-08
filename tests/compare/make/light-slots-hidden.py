# light-slots-hidden: four point lights in a hidden layer, then a sun, the light config counting all five (Ocarina of
# Time's torches and sun). The shaders' four light slots go to the lights that light (owe #65): the sun lights the wall
# and no light is orange.
w = Scene3D('light-slots-hidden', 'four hidden point lights, then a sun', ambient='0.05 0.05 0.05')
lit = w.material('lit', '1 1 1', lighting=True)
w.cube('wall', lit, 0, 0, -1, (0, 0, 0), '12 7 0.2')
w.cube('cube', lit, 0, 0, 1, (0.3, 0.5, 0), '0.8 0.8 0.8')
torches = w.add({"name": "torches", "origin": "0 0 0", "angles": "0 0 0", "scale": "1 1 1", "visible": False})
for i in range(4):
    w.light('torch %d' % i, 'lpoint', -4.5 + i * 3, -2, 3, '1 0.6 0', 6, 20, parent=torches)
w.light('sun', 'ldirectional', 0, 0, 5, '1 1 1', 3, 30, (0.3, 1.2, 0))
w.light_config = lambda: {"point": 4, "spot": 0, "tube": 0, "directional": 1, "directionalshadow": 0}
w.save()

# 22e. model-light-old-kind: does the newer light interface see the old kind of light? A blue "point" light comes
# before a red "lpoint" one, and the light config has one point light: blue, the old kind counts and takes the slot;
# red, the newer interface leaves the old kind out; dark, it leaves it out but the slot is used up
w = Scene3D('model-light-old-kind', 'old kind of light and the newer interface', ambient='0.05 0.05 0.05')
lit = w.material('lit', '1 1 1', lighting=True)
w.cube('wall', lit, 0, 0, -1, (0, 0, 0), '12 7 0.2')
w.light('blue', 'point', -3, 0, 3, '0 0 1', 6, 8)
w.light('red', 'lpoint', 3, 0, 3, '1 0 0', 6, 8)
w.light_config = lambda: {"point": 1, "spot": 0, "tube": 0, "directional": 0}
w.save()

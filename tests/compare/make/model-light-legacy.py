# 22d. model-light-legacy: the old four-light interface (generic2) sees the old kind of light ("point") only: of a
# red "lpoint" and a blue "point" light, only the blue one lights the wall, whatever the light config counts
w = Scene3D('model-light-legacy', 'old light interface', ambient='0.05 0.05 0.05')
old = w.material('old', '1 1 1', lighting=True)
w.files['materials/old.json']['passes'][0].update({"shader": "generic2", "combos": {"LIGHTING": 1}})
w.cube('wall', old, 0, 0, -1, (0, 0, 0), '12 7 0.2')
w.light('red', 'lpoint', -3, 0, 3, '1 0 0', 6, 8)
w.light('blue', 'point', 3, 0, 3, '0 0 1', 6, 8)
w.general['lightconfig'] = {"point": 1, "spot": 0, "tube": 0, "directional": 0}
w.save()

# 22c2. model-light-cookies: two spot lights with different cookies on one wall, with two spot and two spotcookie
# slots: does each project its own cookie?
w = Scene3D('model-light-cookies', 'two spot lights with cookies of their own', ambient='0.05 0.05 0.05')
lit = w.material('lit', '1 1 1', lighting=True)
w.cube('wall', lit, 0, 0, -1, (0, 0, 0), '12 7 0.2')
w.light('left', 'lspot', -3, 0, 8, '1 0.6 0.6', 6, 40, (0, 1.5708, 0), innercone=8, outercone=18,
        usecookie=True, cookie='cookie/flashlight1')
w.light('right', 'lspot', 3, 0, 8, '0.6 0.6 1', 6, 40, (0, 1.5708, 0), innercone=8, outercone=18,
        usecookie=True, cookie='cookie/flashlight3')
w.general['lightconfig'] = {"point": 0, "spot": 2, "tube": 0, "directional": 0, "spotcookie": 2}
w.save()

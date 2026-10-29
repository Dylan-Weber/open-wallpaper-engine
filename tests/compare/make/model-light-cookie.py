# 22c. model-light-cookie: a spot light projecting a texture (a cookie) onto a wall. The real engine draws the cookie
# only when the scene's lightconfig counts it under spotcookie as well as under spot
w = Scene3D('model-light-cookie', 'spot light cookie', ambient='0.05 0.05 0.05')
lit = w.material('lit', '1 1 1', lighting=True)
w.cube('wall', lit, 0, 0, -1, (0, 0, 0), '12 7 0.2')
w.light('light', 'lspot', 0, 0, 8, '1 1 1', 6, 40, (0, 1.5708, 0), innercone=12, outercone=30,
        usecookie=True, cookie='cookie/flashlight1')
w.general['lightconfig'] = {"point": 0, "spot": 1, "tube": 0, "directional": 0, "spotcookie": 1}
w.save()

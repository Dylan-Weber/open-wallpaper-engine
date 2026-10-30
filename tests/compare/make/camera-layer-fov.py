# 46. camera-layer-fov: two hidden camera layers on the default camera, with fields of view of 25 and 75 degrees
# (Ocarina of Time has ten, seven of them hidden). Neither drives the camera, so the scene's 50 stays (owe #64).
w = Scene3D('camera-layer-fov', 'hidden camera layers with fields of view of their own')
for i, color in enumerate(('1 0.3 0.3', '0.3 1 0.3', '0.3 0.3 1')):
    w.cube('cube %d' % i, w.material('cube%d' % i, color), -4 + i * 4, 0, 0, (0.3, 0.6, 0))
events = w.add({"name": "look at events", "origin": "0 0 0", "angles": "0 0 0", "scale": "1 1 1", "visible": False})
w.add({"name": "cam wide", "camera": "default", "origin": "0 0 12", "angles": "0 0 0", "fov": 75, "zoom": 1,
       "parent": events})
w.add({"name": "cam narrow", "camera": "default", "origin": "0 0 12", "angles": "0 0 0", "fov": 25, "zoom": 1,
       "visible": False})
w.save()

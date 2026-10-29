# 22e. camera-fade, -on, -off: a camera path that holds still for 10 seconds, cuts 3 units to the side and holds again
# for 10, then moves back 6 units over 10; camerafade left out, true and false. Stamped with a clock probe: how far the
# camera has come after the cut, and how far it trails the moving segment, at each probed frame.
camera_fade('camera-fade', 'camera path cut, camera fade left out', None)
camera_fade('camera-fade-on', 'camera path cut, camera fade on', True)
camera_fade('camera-fade-off', 'camera path cut, camera fade off', False)

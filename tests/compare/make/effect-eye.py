# 48-49. effect-eye-3d, effect-eye-2d: a fullscreen effect painting g_EyePosition (each component's tenths and
# thousandths): Wallpaper Engine gives an effect the scene camera's eye, the camera path's in 3D and 2000 in front of
# the middle in 2D, where owe's effects saw 0 (owe #61)
w = Scene3D('effect-eye-3d', 'an effect painting the eye: a 3D camera at 1.7 2.3 9.1', eye='1.7 2.3 9.1')
w.cube('cube', w.material('white', '1 1 1'), 0, 0, 0)
eye_wallpaper(w, 0.1)
w = Wallpaper('effect-eye-2d', 'an effect painting the eye of a 2D scene')
w.solid('square', 960, 540, 200, 200, '1 1 1')
eye_wallpaper(w, 0.001)

# fullscreen-material: a fullscreen layer with a material of its own and no effects. Only "passthrough" layers just
# pass the frame on; this one draws (Neon Sunset's sky is one), and a bright marker sits on top (owe #15).
w = Wallpaper('fullscreen-material', 'a fullscreen layer with its own material', clear='0.05 0.05 0.1')
model = w.texture_model('sky', 'util/clouds_256', blending='normal', model_extra={"fullscreen": True})
w.add({"name": "sky", "image": model, "origin": "960 540 0", "size": "1920 1080", "color": "0.4 0.6 1", "alpha": 1,
       "angles": "0 0 0", "scale": "1 1 1"})
w.solid('marker', 960, 540, 300, 300, '1 0.5 0')
w.save()

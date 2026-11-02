# 8. image-blend-modes: the layer blend mode of a layer drawn over a coloured background (modes 0 to 26)
w = Wallpaper('image-blend-modes', 'layer blend modes', clear='0 0 0')
rainbow = w.texture_model('rainbow', 'gradient/gradient_rainbow')
for mode in range(27):
    x = 160 + (mode % 9) * 200
    y = 900 - (mode // 9) * 300
    w.solid('back %d' % mode, x, y, 180, 120, '0.8 0.4 0.2')
    w.image('mode %d' % mode, rainbow, x, y, 180, 120, colorBlendMode=mode)
w.save()

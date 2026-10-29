# 24b. text-size: how big the letters are: an H at four sizes (the real engine's are 25/24 of an em of four units a point)
w = Wallpaper('text-size', 'text sizes: an H at 20, 50, 100 and 200 points', clear='0 0 0')
for pt, x in ((20, 200), (50, 500), (100, 900), (200, 1450)):
    text_layer(w, 'H%d' % pt, x, 540, 'H', 'center', 'center', pointsize=pt, size='600 600', padding=0,
               opaquebackground=False)
w.save()

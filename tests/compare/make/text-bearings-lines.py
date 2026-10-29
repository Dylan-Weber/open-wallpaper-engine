# 24e. text-bearings-lines: lines of different bearings in one layer, aligned left, centred and right
w = Wallpaper('text-bearings-lines', 'text bearings of lines: Lo, j and T in one layer', clear='0 0 0')
for col, h in enumerate(('left', 'center', 'right')):
    text_layer(w, 'lines ' + h, 400 + col * 560, 540, 'Lo\nj\nT', h, 'center', pointsize=60, size='400 700', padding=10)
w.save()

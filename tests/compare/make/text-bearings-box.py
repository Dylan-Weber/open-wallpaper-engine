# 24d. text-bearings-box: the same with their background boxes
w = Wallpaper('text-bearings-box', 'text bearings with boxes: L, j and f', clear='0 0 0')
for row, (h, y) in enumerate((('left', 880), ('center', 540), ('right', 200))):
    for col, letter in enumerate('Ljf'):
        text_layer(w, '%s %s' % (letter, h), 300 + col * 450, y, letter, h, 'center', pointsize=60, size='400 300',
                   padding=10)
w.save()

# 24c. text-bearings: where a line starts and ends: letters whose sides are bare in different amounts (L's right, j's
# left hangs under the pen), each aligned left, centred and right on its origin
w = Wallpaper('text-bearings', 'text bearings: L, j, T and f aligned left, centred and right', clear='0 0 0')
for row, (h, y) in enumerate((('left', 880), ('center', 540), ('right', 200))):
    for col, letter in enumerate('LjTf'):
        text_layer(w, '%s %s' % (letter, h), 300 + col * 450, y, letter, h, 'center', pointsize=60, size='400 300',
                   padding=0, opaquebackground=False)
w.save()

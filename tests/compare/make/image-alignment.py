# 7. image-alignment: where the layer sits relative to its origin (the white marker)
w = Wallpaper('image-alignment', 'alignment', clear='0.1 0.1 0.15')
for i, alignment in enumerate(['center', 'top', 'bottom', 'left', 'right', 'topleft', 'topright', 'bottomleft',
                               'bottomright']):
    x = 300 + (i % 3) * 600
    y = 850 - (i // 3) * 350
    w.solid(alignment, x, y, 200, 120, '0.2 0.6 1', alignment=alignment)
    w.solid('marker ' + alignment, x, y, 12, 12, '1 1 1')
w.save()

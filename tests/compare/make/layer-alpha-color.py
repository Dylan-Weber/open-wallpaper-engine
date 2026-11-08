# 2. layer-alpha-color: opacity steps, colour tints, brightness and translucent overlap
w = Wallpaper('layer-alpha-color', 'alpha and colour', clear='0.2 0.2 0.2')
w.solid('white strip', 960, 780, 1700, 160, '1 1 1')
for i in range(8):
    w.solid('alpha %d' % i, 200 + i * 200, 780, 160, 140, '0 0 0', alpha=round(1 - i / 8, 4))
for i, color in enumerate(['1 0 0', '0 1 0', '0 0 1', '0 1 1', '1 0 1', '1 1 0', '0.5 0.5 0.5', '0.25 0.5 0.75']):
    w.solid('tint %d' % i, 200 + i * 200, 540, 160, 140, color)
w.solid('overlap red', 400, 300, 240, 240, '1 0 0', alpha=0.5)
w.solid('overlap blue', 520, 300, 240, 240, '0 0 1', alpha=0.5)
w.solid('overlap green', 460, 220, 240, 240, '0 1 0', alpha=0.5)
w.solid('brightness half', 1000, 300, 200, 200, '1 1 1', brightness=0.5)
w.solid('brightness full', 1250, 300, 200, 200, '1 1 1', brightness=1.0)
w.solid('hidden', 1500, 300, 200, 200, '1 0 0', visible=False)
w.solid('almost invisible', 1700, 300, 200, 200, '1 1 1', alpha=0.05)
w.save()

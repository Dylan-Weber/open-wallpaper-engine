# 24f. text-box-height: how far a box reaches above and below its text, aligned top, centre and bottom, of one and
# two lines
w = Wallpaper('text-box-height', 'text box heights: one and two lines aligned top, centre and bottom', clear='0 0 0')
for col, (text, v) in enumerate((t, v) for t in ('Hi', 'Hi\nHi') for v in ('top', 'center', 'bottom')):
    text_layer(w, '%s %s' % (text.replace('\n', ' '), v), 160 + col * 320, 540, text, 'center', v, pointsize=36,
               size='300 300', padding=10)
w.save()

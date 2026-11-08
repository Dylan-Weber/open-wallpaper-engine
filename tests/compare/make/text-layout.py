# 17. text-layout: text boxes (the font differs between machines, so look at layout and not at glyph shapes)
w = Wallpaper('text-layout', 'text boxes', clear='0 0 0')
for i, (h, v) in enumerate([('left', 'top'), ('center', 'center'), ('right', 'bottom')]):
    text_layer(w, '%s %s' % (h, v), 350 + i * 600, 850, 'Hello', h, v)
text_layer(w, 'wrapped', 350, 500, 'The quick brown fox jumps over the lazy dog', 'left', 'top',
           limitwidth=True, maxwidth=450, size='500 250', pointsize=36)
text_layer(w, 'rows limited', 950, 500, 'one\ntwo\nthree\nfour', 'center', 'top', limitrows=True, maxrows=2,
           size='500 250', pointsize=36)
text_layer(w, 'padding 60', 1550, 500, 'Pad', 'left', 'top', padding=60)
text_layer(w, 'no background', 350, 150, 'Plain', 'center', 'center', opaquebackground=False)
text_layer(w, 'big', 1100, 150, 'Big', 'center', 'center', pointsize=120, size='700 220')
w.save()

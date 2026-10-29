# 40-43. how loud a band is for how loud a tone (1 kHz, a different level in each channel), and whether the analysis
# has a window: a tone in the middle of an FFT bin (45 x 44100 / 1920 Hz) against one between two bins
for name, left, right in (('audio-level-high', 0.5, 0.2), ('audio-level-mid', 0.08, 0.03),
                          ('audio-level-low', 0.012, 0.005)):
    audio_wallpaper(name, '1 kHz at %g on the left, %g on the right' % (left, right),
                    'aevalsrc=%g*sin(2*PI*1000*t)|%g*sin(2*PI*1000*t):s=48000' % (left, right))
audio_wallpaper('audio-bin-centre', 'equal tones: in the middle of a bin on the left, between two bins on the right',
                'aevalsrc=0.1*sin(2*PI*1033.59*t)|0.1*sin(2*PI*1045.07*t):s=48000')

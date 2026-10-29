# 37-39. audio-*: what the bands show for known sounds
audio_wallpaper('audio-tones', 'steady tones: 100 Hz left, 1 kHz both, 5 kHz right',
                'aevalsrc=0.25*sin(2*PI*100*t)+0.25*sin(2*PI*1000*t)|0.25*sin(2*PI*1000*t)+0.25*sin(2*PI*5000*t):s=48000')
audio_wallpaper('audio-noise', 'pink noise in both channels', 'anoisesrc=color=pink:amplitude=0.3:r=48000,pan=stereo|c0=c0|c1=c0')
audio_wallpaper('audio-quiet-loud', 'one tone quiet on the left, loud on the right',
                'aevalsrc=0.03*sin(2*PI*440*t)|0.5*sin(2*PI*440*t):s=48000')

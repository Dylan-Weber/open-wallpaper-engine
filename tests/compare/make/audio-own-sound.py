# audio-own-sound: script bars (engine.registerAudioBuffers' left and right) while the wallpaper's own sound object plays pink
# noise, nothing else playing. Reze plays its own music and Wallpaper Engine's script-driven bars stay flat there, while the
# probe (the shaders' g_AudioSpectrum) hears the music: do scripts hear a wallpaper's own sound?
import shutil, subprocess
name = 'audio-own-sound'
w = Wallpaper(name, "script audio buffers and the wallpaper's own sound", clear='0 0 0')
# the bars stay between the probe's records (scene y 330 to 740): drawn over them, the recorded bands don't read
script = AUDIO_BARS.replace("""            const height = Math.max(2, values[i] * 400);
            bar.scale = new Vec3(1, height / 2, 1);
            bar.origin = new Vec3(bar.origin.x, (row === 0 ? 600 : 100) + height / 2, 0);""", """            const height = Math.min(180, Math.max(2, values[i] * 180));
            bar.scale = new Vec3(1, height / 2, 1);
            bar.origin = new Vec3(bar.origin.x, (row === 0 ? 560 : 330) + height / 2, 0);""")
assert script != AUDIO_BARS
w.add({"name": "driver", "image": "models/util/solidlayer.json", "size": "10 10", "color": "0 0 0", "alpha": 0,
       "origin": {"value": "10 10 0", "script": script, "scriptproperties": {}}, "angles": "0 0 0",
       "scale": "1 1 1", "alignment": "center"})
w.solid('left baseline', 960, 558, 1800, 2, '0.3 0.3 0.3')
w.solid('right baseline', 960, 328, 1800, 2, '0.3 0.3 0.3')
w.add({"name": "noise", "sound": ["sounds/noise.wav"], "playbackmode": "loop", "volume": 1.0, "startsilent": False,
       "mintime": 1.0, "maxtime": 5.0, "solid": True, "muteineditor": False})
w.save()
project_path = os.path.join(HERE, name, 'project.json')
project = json.load(open(project_path))
project['general']['supportsaudioprocessing'] = True
write(name + '/project.json', project)
path = os.path.join(HERE, name, 'sounds', 'noise.wav')
os.makedirs(os.path.dirname(path), exist_ok=True)
if not os.path.exists(path) and shutil.which('ffmpeg'):
    subprocess.run(['ffmpeg', '-loglevel', 'error', '-y', '-f', 'lavfi', '-i', 'anoisesrc=color=pink:amplitude=0.5', '-t', '4',
                    '-ar', '48000', '-c:a', 'pcm_s16le', path], check=True)
write(name + '/compare.json', {"bar_jitter": 20, "probe_bands": [64]})
clock_probe.stamp(os.path.join(HERE, name))

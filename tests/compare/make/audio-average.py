# audio-average: script bars from engine.registerAudioBuffers' `average` (top row) and `left` (bottom row), with pink noise
# playing. Reze's audio visualizer scales its bars by audioData.average[i] inside try/catch: Wallpaper Engine leaves them
# unscaled (invisible) while owe scales them, as if Wallpaper Engine's buffers had no `average`. The bars stay between the
# probe's records (scene y 330 to 740): drawn over them, the recorded bands don't read and owe hears nothing.
script = AUDIO_BARS.replace("[[0, audio.left], [1, audio.right]]", "[[0, audio.average], [1, audio.left]]")
assert script != AUDIO_BARS
script = script.replace("""            const bar = bars[row * 64 + i];
            const height = Math.max(2, values[i] * 400);
            bar.scale = new Vec3(1, height / 2, 1);
            bar.origin = new Vec3(bar.origin.x, (row === 0 ? 600 : 100) + height / 2, 0);""", """            const bar = bars[row * 64 + i];
            try {
                const height = Math.min(180, Math.max(2, values[i] * 180));
                bar.scale = new Vec3(1, height / 2, 1);
                bar.origin = new Vec3(bar.origin.x, (row === 0 ? 560 : 330) + height / 2, 0);
            } catch (error) { }""")
import shutil, subprocess
name = 'audio-average'
w = Wallpaper(name, 'script audio buffers: average and left', clear='0 0 0')
w.add({"name": "driver", "image": "models/util/solidlayer.json", "size": "10 10", "color": "0 0 0", "alpha": 0,
       "origin": {"value": "10 10 0", "script": script, "scriptproperties": {}}, "angles": "0 0 0",
       "scale": "1 1 1", "alignment": "center"})
w.solid('average baseline', 960, 558, 1800, 2, '0.3 0.3 0.3')
w.solid('left baseline', 960, 328, 1800, 2, '0.3 0.3 0.3')
w.save()
project_path = os.path.join(HERE, name, 'project.json')
project = json.load(open(project_path))
project['general']['supportsaudioprocessing'] = True
write(name + '/project.json', project)
path = os.path.join(HERE, name, 'audio.wav')
if not os.path.exists(path) and shutil.which('ffmpeg'):
    subprocess.run(['ffmpeg', '-loglevel', 'error', '-y', '-f', 'lavfi', '-i', 'anoisesrc=color=pink:amplitude=0.5', '-t', '4',
                    '-ar', '48000', '-c:a', 'pcm_s16le', path], check=True)
write(name + '/.gitignore', 'audio.wav\n')
write(name + '/compare.json', {"bar_jitter": 20, "probe_bands": [64]})
clock_probe.stamp(os.path.join(HERE, name))

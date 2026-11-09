# web-audio-listeners: what a page gets when it registers a second audio listener. Wallpaper Engine keeps one: the
# second registration replaces the first (which is never called again), wallpaperRegisterAudioListener(null) changes
# nothing, and there is no wallpaperRemoveAudioListener. Pages do register twice (Workshop 786782443, VU Meter, at
# start and again in applyGeneralProperties) and write into the array they are given. The "web_eval" value is what
# happened, not how many calls (that is the sound's timing); the page shows the counts.
import shutil
import subprocess

WEB_LISTENERS_PAGE = r'''<!doctype html>
<html><head><meta charset="utf-8"><title>audio listeners</title>
<style>html, body { margin: 0; background: #000; color: #fff; font: 32px sans-serif; }</style></head>
<body><pre id="t"></pre>
<script>
// A at load, B half a second later; A writes into the array it is given and B says whether it saw that. At 5 s
// wallpaperRegisterAudioListener(null), and the calls after it are counted apart.
var r = { a: 0, b: 0, aAfterB: 0, same: 0, sawWrite: 0, afterNull: { a: 0, b: 0 }, nullDone: false, bRegistered: false,
    remove: typeof window.wallpaperRemoveAudioListener };
var lastA = null;
function A(values) {
    if (r.nullDone) r.afterNull.a++; else r.a++;
    if (r.bRegistered) r.aAfterB++;
    lastA = values;
    values[0] = -7;
}
function B(values) {
    if (r.nullDone) r.afterNull.b++; else r.b++;
    if (values === lastA) r.same++;
    if (values[0] === -7) r.sawWrite++;
}
window.wallpaperRegisterAudioListener(A);
setTimeout(function () { window.wallpaperRegisterAudioListener(B); r.bRegistered = true; }, 500);
setTimeout(function () {
    try { window.wallpaperRegisterAudioListener(null); r.nullThrew = false; } catch (e) { r.nullThrew = true; }
    r.nullDone = true;
}, 5000);
window.listenerProbe = function () {
    return { firstCalledAfterSecond: r.aAfterB > 2, secondCalled: r.b > 0, oneArrayForBoth: r.same > 0,
             secondSawFirstsWrite: r.sawWrite > 0, nullThrew: r.nullThrew, calledAfterNull: r.afterNull.a + r.afterNull.b > 0,
             removeAudioListener: r.remove };
};
setInterval(function () { document.getElementById('t').textContent = JSON.stringify(r, null, 1); }, 250);
</script></body></html>
'''

write('web-audio-listeners/index.html', WEB_LISTENERS_PAGE)
write('web-audio-listeners/project.json', {"file": "index.html", "type": "web", "title": "two audio listeners",
                                           "general": {"supportsaudioprocessing": True, "properties": {}}})
write('web-audio-listeners/compare.json', {"web_eval": "window.listenerProbe()", "web_picture": False})
path = os.path.join(HERE, 'web-audio-listeners', 'audio.wav')
if not os.path.exists(path) and shutil.which('ffmpeg'):
    subprocess.run(['ffmpeg', '-loglevel', 'error', '-y', '-f', 'lavfi', '-i',
                    'anoisesrc=color=pink:amplitude=0.04:r=48000,pan=stereo|c0=c0|c1=c0', '-t', '4', '-ar', '48000',
                    '-c:a', 'pcm_s16le', path], check=True)
write('web-audio-listeners/.gitignore', 'audio.wav\n')

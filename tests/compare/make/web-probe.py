# web-*: web wallpapers that paint what Wallpaper Engine hands a page: the audio listener's 128 values, the user
# properties, the window's size and pixel ratio. The page writes them into a record of white and black cells at the
# top left (web_record() reads it), so a screenshot of either engine says what the page was given.
import shutil
import subprocess

WEB_RECORD_MAGIC = 0xA5C3

WEB_PROBE_PAGE = r'''<!doctype html>
<html><head><meta charset="utf-8"><title>web probe</title>
<style>
html, body { margin: 0; width: 100%; height: 100%; overflow: hidden; background: #000; }
canvas { position: absolute; left: 0; top: 0; }
#swatch { position: absolute; left: 1000px; top: 16px; width: 200px; height: 100px; }
#text { position: absolute; left: 1000px; top: 140px; color: #fff; font: 32px sans-serif; }
</style></head>
<body><canvas id="c"></canvas><div id="swatch"></div><div id="text">Web probe 0123456789</div>
<script>
// record layout (record.py reads it): 6-pixel cells, 64 to a row, from (16, 16); each field big-endian
var CELL = 6, PER_ROW = 64, X0 = 16, Y0 = 16;
var latest = null, calls = 0, recent = [], propertyCalls = 0, colour = [0, 0, 0], number = 0, flag = 0;
var generalCalls = 0, fps = 0, pausedCalls = 0, paused = 0;
function onAudio(values) {
    latest = Array.prototype.slice.call(values);
    calls++;
    recent.push(performance.now());
}
if (window.wallpaperRegisterAudioListener) window.wallpaperRegisterAudioListener(onAudio);
window.wallpaperPropertyListener = {
    applyUserProperties: function (p) {
        propertyCalls++;
        if (p.probecolor) {
            colour = p.probecolor.value.split(' ').map(Number);
            document.getElementById('swatch').style.background =
                'rgb(' + colour.map(function (c) { return Math.round(c * 255); }).join(',') + ')';
        }
        if (p.probenumber) number = p.probenumber.value;
        if (p.probebool) flag = p.probebool.value ? 1 : 0;
    },
    applyGeneralProperties: function (p) {
        generalCalls++;
        if (p.fps !== undefined) fps = p.fps;
    },
    setPaused: function (isPaused) {
        pausedCalls++;
        paused = isPaused ? 1 : 0;
    }
};
function put(bits, value, n) {
    value = Math.max(0, Math.min(Math.round(value), Math.pow(2, n) - 1));
    for (var i = n - 1; i >= 0; i--) bits.push(Math.floor(value / Math.pow(2, i)) % 2);
}
function draw() {
    var canvas = document.getElementById('c'), w = window.innerWidth, h = window.innerHeight;
    if (canvas.width !== w || canvas.height !== h) { canvas.width = w; canvas.height = h; }
    var g = canvas.getContext('2d');
    g.fillStyle = '#000';
    g.fillRect(0, 0, w, h);
    var now = performance.now();
    while (recent.length && recent[0] < now - 1000) recent.shift();
    var bits = [];
    put(bits, MAGIC, 16);
    put(bits, w, 16);
    put(bits, h, 16);
    put(bits, window.devicePixelRatio * 1000, 16);
    put(bits, recent.length, 16);           // audio callbacks in the last second
    put(bits, calls, 16);
    put(bits, latest ? latest.length : 0, 16);
    put(bits, propertyCalls, 16);
    put(bits, colour[0] * 255, 8);
    put(bits, colour[1] * 255, 8);
    put(bits, colour[2] * 255, 8);
    put(bits, number, 8);
    put(bits, flag, 16);
    put(bits, generalCalls, 8);
    put(bits, fps, 8);
    put(bits, pausedCalls, 8);
    put(bits, paused, 8);
    for (var i = 0; i < 128; i++) put(bits, (latest && i < latest.length ? latest[i] : 0) * 4096, 16);
    g.fillStyle = '#fff';
    for (var b = 0; b < bits.length; b++) {
        if (bits[b]) g.fillRect(X0 + (b % PER_ROW) * CELL, Y0 + Math.floor(b / PER_ROW) * CELL, CELL, CELL);
    }
    // the values as bars too, left channel above right, 150 pixels to 1 (kept above y 760: the GPU VM's captures
    // have a black box over the bottom left)
    for (var k = 0; latest && k < latest.length; k++) {
        var height = Math.min(latest[k], 2) * 150, x = 16 + (k % 64) * 20, base = k < 64 ? h * 0.52 : h * 0.83;
        g.fillStyle = k < 64 ? '#4cf' : '#fc4';
        g.fillRect(x, base - height, 16, height);
    }
    requestAnimationFrame(draw);
}
requestAnimationFrame(draw);
</script></body></html>
'''.replace('MAGIC', str(WEB_RECORD_MAGIC))


def web_wallpaper(name, title, sound=None):
    write(name + '/index.html', WEB_PROBE_PAGE)
    general = {"properties": {
        "probecolor": {"type": "color", "value": "0.2 0.6 1", "text": "colour", "order": 0},
        "probenumber": {"type": "slider", "value": 37, "min": 0, "max": 100, "text": "number", "order": 1},
        "probebool": {"type": "bool", "value": True, "text": "flag", "order": 2}}}
    if sound:
        general["supportsaudioprocessing"] = True
    write(name + '/project.json', {"file": "index.html", "type": "web", "title": title, "general": general})
    settings = {"web_probe": True}
    if sound:
        path = os.path.join(HERE, name, 'audio.wav')
        if not os.path.exists(path) and shutil.which('ffmpeg'):
            subprocess.run(['ffmpeg', '-loglevel', 'error', '-y', '-f', 'lavfi', '-i', sound, '-t', '4',
                            '-ar', '48000', '-c:a', 'pcm_s16le', path], check=True)
        write(name + '/.gitignore', 'audio.wav\n')
    write(name + '/compare.json', settings)


web_wallpaper('web-layout', 'a page with no sound: window size, pixel ratio, user properties')
web_wallpaper('web-audio-tones', 'steady tones through the web audio listener: 100 Hz left, 1 kHz both, 5 kHz right',
              'aevalsrc=0.25*sin(2*PI*100*t)+0.25*sin(2*PI*1000*t)|0.25*sin(2*PI*1000*t)+0.25*sin(2*PI*5000*t):s=48000')
web_wallpaper('web-audio-noise', 'pink noise through the web audio listener',
              'anoisesrc=color=pink:amplitude=0.04:r=48000,pan=stereo|c0=c0|c1=c0')
web_wallpaper('web-audio-quiet-loud', 'one tone quiet on the left, loud on the right, through the web audio listener',
              'aevalsrc=0.03*sin(2*PI*440*t)|0.5*sin(2*PI*440*t):s=48000')

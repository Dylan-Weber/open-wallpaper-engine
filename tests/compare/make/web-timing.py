# web-raf-rate: how often a web wallpaper's page is drawn (requestAnimationFrame calls a second) against the frame rate
# applyGeneralProperties tells it. Wallpaper Engine tells the page its fps setting but draws it at the display's rate:
# 63-64 calls a second in the VM with the setting at 15 and at 30. The "web_eval" value says only whether the page is
# drawn more than twice as often as it is told (the exact rate is the machine's); the page shows the numbers.
WEB_RAF_PAGE = r'''<!doctype html>
<html><head><meta charset="utf-8"><title>raf rate</title>
<style>html, body { margin: 0; background: #000; color: #fff; font: 40px sans-serif; }</style></head>
<body><div id="t">raf</div>
<script>
var raf = [], told = 0;
window.wallpaperPropertyListener = { applyGeneralProperties: function (p) { if (p.fps !== undefined) told = p.fps; } };
function frame() { raf.push(performance.now()); requestAnimationFrame(frame); }
requestAnimationFrame(frame);
window.rafCounts = function () {
    var now = performance.now();
    return { raf: raf.filter(function (x) { return x > now - 1000; }).length, told: told };
};
window.rafProbe = function () {
    var c = window.rafCounts();
    return { told: c.told, drawnFasterThanTold: c.raf > 2 * c.told };
};
setInterval(function () {
    var c = window.rafCounts();
    document.getElementById('t').textContent = 'requestAnimationFrame ' + c.raf + ' a second, told fps ' + c.told;
}, 250);
</script></body></html>
'''

write('web-raf-rate/index.html', WEB_RAF_PAGE)
write('web-raf-rate/project.json', {"file": "index.html", "type": "web", "title": "requestAnimationFrame rate against the fps told",
                                    "general": {"properties": {}}})
write('web-raf-rate/compare.json', {"web_eval": "window.rafProbe()", "web_picture": False})

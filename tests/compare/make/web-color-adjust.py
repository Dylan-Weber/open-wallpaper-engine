# web-color-adjust*: Wallpaper Engine's own flip and colour adjustment of a web wallpaper. It adds the properties
# alignmentfliph and wec_e (show colour options), wec_brs (brightness), wec_con (contrast), wec_sa (saturation),
# wec_hue (hue shift), sliders 0-100 at 50, to what the page is handed (web-property-shapes) and applies them itself. The
# page paints known patches: greys, primaries and secondaries, a hue ramp, and an asymmetric L for the flip; each
# variant sets the adjustment as Wallpaper Engine's panel does (compare.json "we_properties", capture.ps1 runs -control
# applyProperties; a project.json default for these keys is ignored). "web_eval" is the last value the page was handed.
WEB_ADJUST_PAGE = r'''<!doctype html>
<html><head><meta charset="utf-8"><title>colour adjust</title>
<style>html, body { margin: 0; background: #000; overflow: hidden; } div { position: absolute; }</style></head>
<body>
<script>
// 100x100 patches from (100, 100): row 1 greys 0..255 in 8 steps, row 2 R G B C M Y and two mixed colours, row 3 a
// 12-step hue ramp at full saturation, row 4 the same at half saturation and value; an L at the top left for the flip
function box(x, y, w, h, colour) {
    var d = document.createElement('div');
    d.style.left = x + 'px'; d.style.top = y + 'px'; d.style.width = w + 'px'; d.style.height = h + 'px';
    d.style.background = colour; document.body.appendChild(d);
}
var greys = [0, 36, 73, 109, 146, 182, 219, 255];
greys.forEach(function (g, i) { box(100 + i * 110, 100, 100, 100, 'rgb(' + g + ',' + g + ',' + g + ')'); });
['#f00', '#0f0', '#00f', '#0ff', '#f0f', '#ff0', 'rgb(200,120,40)', 'rgb(40,90,160)'].forEach(function (c, i) {
    box(100 + i * 110, 220, 100, 100, c); });
for (var i = 0; i < 12; i++) {
    box(100 + i * 110, 340, 100, 100, 'hsl(' + (i * 30) + ',100%,50%)');
    box(100 + i * 110, 460, 100, 100, 'hsl(' + (i * 30) + ',50%,25%)');
}
box(20, 20, 60, 10, '#fff'); box(20, 20, 10, 60, '#fff');
var handed = null;
var calls = 0;
window.wallpaperPropertyListener = { applyUserProperties: function (p) {
    calls++;
    handed = handed || {};
    ['alignmentfliph', 'wec_e', 'wec_brs', 'wec_con', 'wec_sa', 'wec_hue'].forEach(function (k) {
        if (p[k]) handed[k] = p[k].value; else if (!(k in handed)) handed[k] = 'missing'; });
    handed.calls = calls;
} };
window.adjustProbe = function () {
    function look(e) { var c = getComputedStyle(e); return { filter: c.filter, transform: c.transform, style: e.getAttribute('style') }; }
    return { handed: handed, html: look(document.documentElement), body: look(document.body) };
};
</script></body></html>
'''

ADJUST_VARIANTS = {
    '': {},
    '-flip': {'alignmentfliph': True},
    '-on': {'wec_e': True},
    '-brs-0': {'wec_e': True, 'wec_brs': 0}, '-brs-25': {'wec_e': True, 'wec_brs': 25},
    '-brs-75': {'wec_e': True, 'wec_brs': 75}, '-brs-100': {'wec_e': True, 'wec_brs': 100},
    '-con-0': {'wec_e': True, 'wec_con': 0}, '-con-25': {'wec_e': True, 'wec_con': 25},
    '-con-75': {'wec_e': True, 'wec_con': 75}, '-con-100': {'wec_e': True, 'wec_con': 100},
    '-sa-0': {'wec_e': True, 'wec_sa': 0}, '-sa-25': {'wec_e': True, 'wec_sa': 25},
    '-sa-75': {'wec_e': True, 'wec_sa': 75}, '-sa-100': {'wec_e': True, 'wec_sa': 100},
    '-hue-0': {'wec_e': True, 'wec_hue': 0}, '-hue-25': {'wec_e': True, 'wec_hue': 25},
    '-hue-75': {'wec_e': True, 'wec_hue': 75}, '-hue-100': {'wec_e': True, 'wec_hue': 100},
    '-combo': {'wec_e': True, 'wec_brs': 65, 'wec_con': 35, 'wec_sa': 70, 'wec_hue': 60},
    '-off-brs-100': {'wec_brs': 100},
    '-flip-on': {'alignmentfliph': True, 'wec_e': True},
}

for suffix, values in ADJUST_VARIANTS.items():
    name = 'web-color-adjust' + suffix
    write(name + '/index.html', WEB_ADJUST_PAGE)
    write(name + '/project.json', {"file": "index.html", "type": "web", "title": "colour adjust " + (suffix or 'none'),
                                   "general": {"properties": {}}})
    settings = {"web_eval": "window.adjustProbe()"}
    if values:
        settings["we_properties"] = values
    write(name + '/compare.json', settings)

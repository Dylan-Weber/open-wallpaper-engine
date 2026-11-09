# web-webgl: does a WebGL canvas reach the picture? Two canvases cleared to colours every frame, one with
# preserveDrawingBuffer false (the default, Fluid 1748506393's) and one true, plus a 2D canvas. Two Workshop WebGL pages
# picture black in both engines; this says whether that is the capture or the pages.
WEB_WEBGL_PAGE = r'''<!doctype html>
<html><head><meta charset="utf-8"><title>webgl</title>
<style>html, body { margin: 0; background: #000; } canvas { position: absolute; top: 100px; width: 400px; height: 300px; }
#a { left: 100px; } #b { left: 600px; } #c { left: 1100px; }</style></head>
<body><canvas id="a" width="400" height="300"></canvas><canvas id="b" width="400" height="300"></canvas>
<canvas id="c" width="400" height="300"></canvas>
<script>
var r = { frames: 0 };
function gl(id, preserve) {
    var c = document.getElementById(id);
    var g = c.getContext('webgl', { preserveDrawingBuffer: preserve });
    r[id] = g ? { renderer: g.getParameter(g.RENDERER), lost: g.isContextLost() } : null;
    return g;
}
var a = gl('a', false), b = gl('b', true);
var c2 = document.getElementById('c').getContext('2d');
function frame() {
    r.frames++;
    if (a) { a.clearColor(1, 0.5, 0, 1); a.clear(a.COLOR_BUFFER_BIT); }
    if (b) { b.clearColor(0, 0.75, 1, 1); b.clear(b.COLOR_BUFFER_BIT); }
    c2.fillStyle = '#4c4'; c2.fillRect(0, 0, 400, 300);
    requestAnimationFrame(frame);
}
requestAnimationFrame(frame);
window.webglProbe = function () { return { a: !!r.a, b: !!r.b, drawing: r.frames > 10 }; };
</script></body></html>
'''

write('web-webgl/index.html', WEB_WEBGL_PAGE)
write('web-webgl/project.json', {"file": "index.html", "type": "web", "title": "WebGL canvases", "general": {"properties": {}}})
write('web-webgl/compare.json', {"web_eval": "window.webglProbe()"})

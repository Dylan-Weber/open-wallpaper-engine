# web-property-dump*: the whole first applyUserProperties argument for a few project.json shapes, to see which of
# Wallpaper Engine's own properties it adds (web-property-shapes: _d0, alignmentfliph, wec_*) and how it renumbers the
# wallpaper's own orders: no properties; one property and no schemecolor; a property without an order.
WEB_DUMP_PAGE = r'''<!doctype html>
<html><head><meta charset="utf-8"><title>property dump</title>
<style>html, body { margin: 0; background: #000; color: #fff; font: 14px monospace; }</style></head>
<body><pre id="t"></pre>
<script>
var first = null, calls = 0;
window.wallpaperPropertyListener = { applyUserProperties: function (p) {
    calls++; if (!first) first = JSON.parse(JSON.stringify(p)); } };
window.dumpProbe = function () { return { calls: calls, first: first, keys: first ? Object.keys(first) : null }; };
setInterval(function () { document.getElementById('t').textContent = JSON.stringify(window.dumpProbe(), null, 1); }, 500);
</script></body></html>
'''

for suffix, properties in {
    '-empty': {},
    '-noscheme': {"one": {"order": 5, "text": "one", "type": "slider", "value": 3, "min": 0, "max": 10}},
    '-noorder': {"schemecolor": {"text": "ui_browse_properties_scheme_color", "type": "color", "value": "1 0 0"},
                 "plain": {"text": "plain", "type": "bool", "value": True},
                 "negative": {"order": -3, "text": "negative", "type": "bool", "value": False}},
}.items():
    name = 'web-property-dump' + suffix
    write(name + '/index.html', WEB_DUMP_PAGE)
    write(name + '/project.json', {"file": "index.html", "type": "web", "title": "property dump " + suffix,
                                   "general": {"properties": properties}})
    write(name + '/compare.json', {"web_eval": "window.dumpProbe()", "web_picture": False})

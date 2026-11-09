# web-property-shapes: exactly what a page is handed, for every user property type: the first applyUserProperties
# argument as JSON (keys, their order, each entry's fields and value types), how many times each listener is called,
# what wallpaperRequestRandomFileForProperty calls back with (a directory property with no folder chosen, and a name
# that isn't a property), and the plugins wallpaperPluginListener.onPluginLoaded is told of.
WEB_SHAPES_PAGE = r'''<!doctype html>
<html><head><meta charset="utf-8"><title>property shapes</title>
<style>html, body { margin: 0; background: #000; color: #fff; font: 14px monospace; }</style></head>
<body><pre id="t"></pre>
<script>
var r = { user: [], general: [], paused: [], plugins: [], random: [] };
window.wallpaperPropertyListener = {
    applyUserProperties: function (p) { r.user.push(JSON.parse(JSON.stringify(p))); },
    applyGeneralProperties: function (p) { r.general.push(JSON.parse(JSON.stringify(p))); },
    setPaused: function (v) { r.paused.push(v); }
};
window.wallpaperPluginListener = { onPluginLoaded: function (name, version) { r.plugins.push([name, typeof version]); } };
function ask(name) {
    try {
        window.wallpaperRequestRandomFileForProperty(name, function (property, file) {
            r.random.push({ asked: name, property: property, file: file === undefined ? 'undefined' : file });
        });
    } catch (e) { r.random.push({ asked: name, threw: String(e) }); }
}
setTimeout(function () { ask('folder'); ask('nosuchproperty'); }, 1000);
window.shapesProbe = function () { return r; };
setInterval(function () { document.getElementById('t').textContent = JSON.stringify(r, null, 1); }, 500);
</script></body></html>
'''

write('web-property-shapes/index.html', WEB_SHAPES_PAGE)
write('web-property-shapes/project.json', {"file": "index.html", "type": "web", "title": "every property type", "general": {
    "properties": {
        "schemecolor": {"order": 0, "text": "ui_browse_properties_scheme_color", "type": "color", "value": "0.5 0.25 1"},
        "aslider": {"order": 1, "text": "slider", "type": "slider", "value": 37, "min": 0, "max": 100, "step": 1,
                    "editable": True, "fraction": False},
        "afraction": {"order": 2, "text": "fraction", "type": "slider", "value": 0.25, "min": 0, "max": 1,
                      "fraction": True, "precision": 2},
        "abool": {"order": 3, "text": "bool", "type": "bool", "value": True},
        "acombo": {"order": 4, "text": "combo", "type": "combo", "value": 2,
                   "options": [{"label": "one", "value": 1}, {"label": "two", "value": 2}]},
        "astringcombo": {"order": 5, "text": "string combo", "type": "combo", "value": "b",
                         "options": [{"label": "A", "value": "a"}, {"label": "B", "value": "b"}]},
        "atext": {"order": 6, "text": "a label only", "type": "text"},
        "atextinput": {"order": 7, "text": "text input", "type": "textinput", "value": "hello"},
        "afile": {"order": 8, "text": "file", "type": "file", "value": ""},
        "folder": {"order": 9, "text": "directory", "type": "directory", "mode": "fetchall", "value": ""},
        "acondition": {"order": 10, "text": "shown if bool", "type": "slider", "value": 5, "min": 0, "max": 10,
                       "condition": "abool.value == true"},
        "zlast": {"order": 11, "text": "last", "type": "bool", "value": False}}}})
write('web-property-shapes/compare.json', {"web_eval": "window.shapesProbe()", "web_picture": False})

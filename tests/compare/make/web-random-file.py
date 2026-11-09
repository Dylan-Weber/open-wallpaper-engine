# web-random-file: what wallpaperRequestRandomFileForProperty does with no folder chosen. Wallpaper Engine calls back
# (propertyName, "") for a directory property with no folder and for a name that is no property (web-property-shapes);
# this records whether the callback runs before the call returns, its arguments, and a call without a callback.
WEB_RANDOM_PAGE = r'''<!doctype html>
<html><head><meta charset="utf-8"><title>random file</title>
<style>html, body { margin: 0; background: #000; color: #fff; font: 20px monospace; }</style></head>
<body><pre id="t"></pre>
<script>
var r = { calls: [] };
function ask(name, label) {
    var returned = false, result;
    try {
        result = window.wallpaperRequestRandomFileForProperty(name, function () {
            r.calls.push({ asked: label, beforeReturn: !returned, args: Array.prototype.map.call(arguments, function (a) {
                return a === undefined ? 'undefined' : a; }) });
        });
        returned = true;
        r[label + 'Returned'] = result === undefined ? 'undefined' : result;
    } catch (e) { r[label + 'Threw'] = String(e); }
}
setTimeout(function () {
    ask('folder', 'folder');
    ask('nosuchproperty', 'nosuch');
    try { window.wallpaperRequestRandomFileForProperty('folder'); r.noCallback = 'ok'; } catch (e) { r.noCallback = String(e); }
}, 1000);
window.randomProbe = function () { return r; };
setInterval(function () { document.getElementById('t').textContent = JSON.stringify(r, null, 1); }, 500);
</script></body></html>
'''

write('web-random-file/index.html', WEB_RANDOM_PAGE)
write('web-random-file/project.json', {"file": "index.html", "type": "web", "title": "random file, no folder", "general": {
    "properties": {"folder": {"order": 0, "text": "directory", "type": "directory", "mode": "fetchall", "value": ""}}}})
write('web-random-file/compare.json', {"web_eval": "window.randomProbe()", "web_picture": False})

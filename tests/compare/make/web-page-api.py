# web-page-api: a page that calls the rest of Wallpaper Engine's page API without checking for it first (media
# listeners, utilities, random files, video ended, screenshot response, media integration constants) and shows, per
# call, "ok" (green) or the error it threw (red). window.pageApiResults holds the same for DevTools.
PAGE_API_CALLS = [
    "wallpaperRegisterMediaPropertiesListener(function(e){})",
    "wallpaperRegisterMediaThumbnailListener(function(e){})",
    "wallpaperRegisterMediaPlaybackListener(function(e){})",
    "wallpaperRegisterMediaTimelineListener(function(e){})",
    "wallpaperRegisterMediaStatusListener(function(e){})",
    "wallpaperGetUtilities()",
    "wallpaperRequestRandomFileForProperty('folder', function(name, file){})",
    "wallpaperOnVideoEnded()",
    "wallpaperRequestTakeScreenshotResponse()",
    "window.wallpaperMediaIntegration.PLAYBACK_PLAYING",
]
rows = ''.join('''  try { %s; ok(%d); } catch (e) { fail(%d, e); }\n''' % (call, i, i) for i, call in enumerate(PAGE_API_CALLS))
page = '''<!doctype html>
<html><head><meta charset="utf-8"><title>page api</title>
<style>html, body { margin: 0; background: #000; color: #fff; font: 24px monospace; }
div { height: 60px; padding-left: 16px; line-height: 60px; }</style></head>
<body>
<script>
var calls = %s;
window.pageApiResults = [];
function row(i, text, colour) {
  var d = document.createElement('div'); d.textContent = calls[i] + ': ' + text; d.style.background = colour;
  document.body.appendChild(d); window.pageApiResults.push(calls[i] + ': ' + text);
}
function ok(i) { row(i, 'ok', '#174'); }
function fail(i, e) { row(i, String(e), '#a22'); }
%s</script></body></html>
''' % (json.dumps(PAGE_API_CALLS), rows)
write('web-page-api/index.html', page)
write('web-page-api/project.json', {"file": "index.html", "type": "web", "title": "wpe-test the rest of the page API, called unguarded"})

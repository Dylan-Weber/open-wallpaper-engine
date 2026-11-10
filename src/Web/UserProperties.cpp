module weweb;

import rstd;
import rstd.cppstd;

import :cef;
import :cef_internal;
using namespace rstd::prelude;
using namespace rstd::literals;
using rstd::cppstd::to_string;
using rstd::fmt::Arguments;
using rstd::fmt::Formatter;

namespace weweb
{

String BuildPropertyListenerSnippet(const owe::Json& props) {
    // The page side typically registers a listener like:
    //
    //   window.wallpaperPropertyListener = {
    //     applyUserProperties: function(props) { ... }
    //   };
    //
    // We hand it the project.json `general.properties` object verbatim;
    // each entry preserves its `type` and `value` fields, matching what
    // WE's own runtime delivers.
    String snippet =
        "(function(){"
        "  if (typeof window.wallpaperPropertyListener !== 'object') return;"
        "  if (typeof window.wallpaperPropertyListener.applyUserProperties !== 'function') return;"
        "  try {"
        "    window.wallpaperPropertyListener.applyUserProperties("_Str;
    auto json = props.is_object() ? owe::DumpString(props) : "{}"_Str;
    snippet.push_str(json.as_str());
    snippet.push_str("    );"
                     "  } catch (e) {"
                     "    console.error('weweb: applyUserProperties threw:', e);"
                     "  }"
                     "})();"_str);
    return snippet;
}

String BuildPropertyPatchSnippet(ref<str> key, const owe::Json& value) {
    auto object = rstd::json::Map::make();
    object.insert(String::make(key), value.clone());
    auto properties = owe::Json::Object(rstd::move(object));
    auto snippet =
        "(function(){"
        "  if (typeof window.wallpaperPropertyListener !== 'object') return;"
        "  if (typeof window.wallpaperPropertyListener.applyUserProperties !== 'function') return;"
        "  try {"
        "    window.wallpaperPropertyListener.applyUserProperties("_Str;
    snippet.push_str(owe::DumpString(properties).as_str());
    snippet.push_str(");"
                     "  } catch (e) {"
                     "    console.error('weweb: applyUserProperties patch threw:', e);"
                     "  }"
                     "})();"_str);
    return snippet;
}

String BuildAudioResponseSnippet(slice<float> data) {
    auto snippet = "(function(){if(!window.__weweb_pushAudio)return;window.__weweb_pushAudio(["_Str;
    Formatter formatter(snippet);
    bool      first = true;
    for (float value : data) {
        if (! first) snippet.push_ascii(',');
        first       = false;
        auto number = f32(value);
        if (number.is_nan()) {
            snippet.push_str("NaN"_str);
        } else if (! number.is_finite()) {
            snippet.push_str(number < f32() ? "-Infinity"_str : "Infinity"_str);
        } else {
            formatter.write_fmt(Arguments::make("{:.8e}", number));
        }
    }
    snippet.push_str("]);})();"_str);
    return snippet;
}

// Wallpaper Engine calls applyGeneralProperties with its whole general config once the page has
// loaded: 60 keys (read over DevTools in the VM, probe web-general-dump). owe gives its frame rate
// and, for the rest, Wallpaper Engine's defaults and the values it ran with in the VM, except what
// belongs to that machine (the monitor map, the CEF command line, the apps list), which is empty.
String BuildGeneralPropertiesJson(int fps) {
    String json =
        R"JSON({"adjustdwmcolormode":"disabled","anticrash":null,"apprules":[],"audioinputdevice":"default","audioinputthreshold":0,"audioinputvolume":50,"autostart":false,"autostartscheduler":false,"autostartx64":false,"browsetransition":{"transition":"none","transitiontime":1500},"cefcommandline":"",)JSON"_Str;
    Formatter formatter(json);
    json.push_str("\"fps\":"_str);
    formatter.write_fmt(Arguments::make("{}", fps));
    json.push_str(
        R"JSON(,"hasshownwelcomedialog":true,"highprecisiontimer":true,"iconopacity":100,"language":"en-us","location":{},"loglevel":"error","mediablocklist":null,"mediaintegration":true,"monitordetection":"managed","monitormap":{},"msaa":"none","overridelockscreen":false,"overridewallpaper":false,"pausevram":false,"playbackaudio":"run","playbackfocus":"run","playbackfullscreen":"run","playbackmaximized":"run","playbackonbattery":"run","playbacksleep":"run","plugindelay":0,"plugins":{"ledextensions":{"enabled":true}},"postprocessing":"enabled","preset":"medium","processpriority":"normal","reflection":true,"reloadaudio":true,"resolution":"full","safemode":false,"shadows":"medium","slideshowkiller":false,"uicmd":"","uieffects":null,"uihardwareacceleration":true,"uiquality":"high","uiskin":"dark","uiskinseasonal":true,"unpauseaero":true,"vdesktopenabled":false,"videoaudiooutput":true,"videoframework":"mfEngine","videohardwareacceleration":true,"videoloopmode":"default","videomfstutterhack":false,"videoreadmode":"fromdisk","volumetrics":"medium","webmframework":null,"windowupdaterate":1000})JSON"_str);
    return json;
}

String BuildGeneralPropertiesSnippet(int fps) {
    auto snippet = "(function(){"
                   "  if (typeof window.wallpaperPropertyListener !== 'object') return;"
                   "  if (typeof window.wallpaperPropertyListener.applyGeneralProperties !== "
                   "'function') return;"
                   "  try {"
                   "    window.wallpaperPropertyListener.applyGeneralProperties("_Str;
    snippet.push_str(BuildGeneralPropertiesJson(fps).as_str());
    snippet.push_str(");"
                     "  } catch (e) {"
                     "    console.error('weweb: applyGeneralProperties threw:', e);"
                     "  }"
                     "})();"_str);
    return snippet;
}

// Wallpaper Engine puts a style element with these rules into every page's head just after it
// loads, before the first applyUserProperties (read over DevTools in the VM): pausing then sets the
// class wpxPausePseudoAnimationAll on <html> (BuildPausedSnippet).
String BuildPausePseudoAnimationsSnippet() {
    return "(function(){"
           "  if (window.__weweb_pause_style) return;"
           "  window.__weweb_pause_style = true;"
           "  var style = document.createElement('style');"
           "  style.setAttribute('type', 'text/css');"
           "  style.textContent ="
           "    '.wpxPausePseudoAnimations:before{animation-play-state:paused !important}'"
           "    + '.wpxPausePseudoAnimations:after{animation-play-state:paused !important}'"
           "    + '.wpxPausePseudoAnimationAll *{animation-play-state:paused !important}'"
           "    + '.wpxPausePseudoAnimationAll *:before{animation-play-state:paused !important}'"
           "    + '.wpxPausePseudoAnimationAll *:after{animation-play-state:paused !important}';"
           "  (document.head || document.documentElement).appendChild(style);"
           "})();"_Str;
}

// What Wallpaper Engine does when it pauses or resumes a page (read over DevTools): setPaused,
// then <html> gets the class wpxPausePseudoAnimationAll and every element an inline
// animation-play-state: paused, which resuming clears again. The page's setPaused is optional.
// Wallpaper Engine then stops the page's process outright (timers and animation frames included,
// visibilityState stays "visible", no event): __weweb_pauseAck tells the browser process the page
// has seen it all, and it stops the renderer.
String BuildPausedSnippet(bool paused) {
    String snippet = "(function(){"
                     "  var paused = "_Str;
    snippet.push_str(paused ? "true;"_str : "false;"_str);
    snippet.push_str(
        "  var listener = window.wallpaperPropertyListener;"
        "  if (typeof listener === 'object' && listener && typeof listener.setPaused === "
        "'function') {"
        "    try { listener.setPaused(paused); }"
        "    catch (e) { console.error('weweb: setPaused threw:', e); }"
        "  }"
        "  var root = document.documentElement;"
        "  if (root) {"
        "    root.classList.toggle('wpxPausePseudoAnimationAll', paused);"
        "    var all = document.querySelectorAll('*');"
        "    root.style.animationPlayState = paused ? 'paused' : '';"
        "    for (var i = 0; i < all.length; i++) all[i].style.animationPlayState = paused ? "
        "'paused' : '';"
        "  }"
        // everything the page was to see is done: the browser may now stop the renderer
        "  if (paused && typeof window.__weweb_pauseAck === 'function') window.__weweb_pauseAck();"
        "})();"_str);
    return snippet;
}

void InjectUserProperties(CefRefPtr<CefBrowser> browser, const owe::Json& props) {
    if (! browser) return;
    auto frame = browser->GetMainFrame();
    if (! frame) return;
    frame->ExecuteJavaScript(to_string(BuildPropertyListenerSnippet(props).as_str()),
                             "weweb://internal/inject_user_properties.js",
                             0);
}

} // namespace weweb

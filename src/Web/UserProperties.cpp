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

// Wallpaper Engine calls applyGeneralProperties({fps}) with its frame rate setting once the page
// has loaded.
String BuildGeneralPropertiesSnippet(int fps) {
    auto      snippet = "(function(){"
                        "  if (typeof window.wallpaperPropertyListener !== 'object') return;"
                        "  if (typeof window.wallpaperPropertyListener.applyGeneralProperties !== "
                        "'function') return;"
                        "  try {"
                        "    window.wallpaperPropertyListener.applyGeneralProperties({\"fps\":"_Str;
    Formatter formatter(snippet);
    formatter.write_fmt(Arguments::make("{}", fps));
    snippet.push_str("});"
                     "  } catch (e) {"
                     "    console.error('weweb: applyGeneralProperties threw:', e);"
                     "  }"
                     "})();"_str);
    return snippet;
}

// The rest of the page API Wallpaper Engine puts on window (read over DevTools in its CEF): media
// integration listeners, utilities, random files from a directory property, video-ended and
// screenshot responses. owe has none of those services, so each is defined with the same type and
// does nothing: a registration keeps nothing and never calls back, so a page that calls one without
// checking for it doesn't throw. The playback constants are the names pages compare media states
// with (their values weren't measured; owe never sends a state).
String BuildPageApiSnippet() {
    return "(function(){"
           "  function define(name, value) {"
           "    if (typeof window[name] === 'undefined') window[name] = value;"
           "  }"
           "  define('wallpaperRegisterMediaPropertiesListener', function(callback){});"
           "  define('wallpaperRegisterMediaThumbnailListener', function(callback){});"
           "  define('wallpaperRegisterMediaPlaybackListener', function(callback){});"
           "  define('wallpaperRegisterMediaTimelineListener', function(callback){});"
           "  define('wallpaperRegisterMediaStatusListener', function(callback){});"
           "  define('wallpaperGetUtilities', function(){ return {}; });"
           "  define('wallpaperRequestRandomFileForProperty', function(propertyName, callback){});"
           "  define('wallpaperOnVideoEnded', function(){});"
           "  define('wallpaperRequestTakeScreenshotResponse', function(){});"
           "  define('wallpaperMediaIntegration', {"
           "    PLAYBACK_STOPPED: 0, PLAYBACK_PLAYING: 1, PLAYBACK_PAUSED: 2"
           "  });"
           "})();"_Str;
}

// Wallpaper Engine's web audio-response API, installed before the page's own scripts run. The page
// calls wallpaperRegisterAudioListener to subscribe; the browser process feeds samples each tick by
// invoking __weweb_pushAudio with a 128-float array (64 left + 64 right). Values remain linear and
// may exceed 1. As in Wallpaper Engine (read over DevTools) the page has one listener: registering
// again replaces it, registering anything but a function changes nothing, and there is no way to
// remove it.
String BuildAudioApiSnippet() {
    return "(function(){"
           "  if (window.__weweb_audio_installed) return;"
           "  window.__weweb_audio_installed = true;"
           "  var listener = null;"
           "  window.wallpaperRegisterAudioListener = function(cb){"
           "    if (typeof cb !== 'function') return;"
           "    if (listener === null) window.__weweb_setAudioDemand(true);"
           "    listener = cb;"
           "  };"
           "  window.__weweb_pushAudio = function(arr){"
           "    if (listener === null) return;"
           "    try { listener(arr); } catch (e) {}"
           "  };"
           "})();"_Str;
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

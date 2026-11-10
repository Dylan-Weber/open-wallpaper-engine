module weweb;

import rstd;
import rstd.cppstd;

import :cef;
import :cef_internal;
import :browser_host;
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
    String json = R"JSON({"adjustdwmcolormode":"disabled","anticrash":null,"apprules":[],"audioinputdevice":"default","audioinputthreshold":0,"audioinputvolume":50,"autostart":false,"autostartscheduler":false,"autostartx64":false,"browsetransition":{"transition":"none","transitiontime":1500},"cefcommandline":"",)JSON"_Str;
    Formatter formatter(json);
    json.push_str("\"fps\":"_str);
    formatter.write_fmt(Arguments::make("{}", fps));
    json.push_str(R"JSON(,"hasshownwelcomedialog":true,"highprecisiontimer":true,"iconopacity":100,"language":"en-us","location":{},"loglevel":"error","mediablocklist":null,"mediaintegration":true,"monitordetection":"managed","monitormap":{},"msaa":"none","overridelockscreen":false,"overridewallpaper":false,"pausevram":false,"playbackaudio":"run","playbackfocus":"run","playbackfullscreen":"run","playbackmaximized":"run","playbackonbattery":"run","playbacksleep":"run","plugindelay":0,"plugins":{"ledextensions":{"enabled":true}},"postprocessing":"enabled","preset":"medium","processpriority":"normal","reflection":true,"reloadaudio":true,"resolution":"full","safemode":false,"shadows":"medium","slideshowkiller":false,"uicmd":"","uieffects":null,"uihardwareacceleration":true,"uiquality":"high","uiskin":"dark","uiskinseasonal":true,"unpauseaero":true,"vdesktopenabled":false,"videoaudiooutput":true,"videoframework":"mfEngine","videohardwareacceleration":true,"videoloopmode":"default","videomfstutterhack":false,"videoreadmode":"fromdisk","volumetrics":"medium","webmframework":null,"windowupdaterate":1000})JSON"_str);
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

// The rest of the page API Wallpaper Engine puts on window (read over DevTools in its CEF): media
// integration listeners, utilities, random files from a directory property, video-ended and
// screenshot responses. owe has none of those services, so each is defined with the same type and
// does nothing: a registration keeps nothing and never calls back, so a page that calls one without
// checking for it doesn't throw. A random file is asked for with no folder chosen, which Wallpaper
// Engine answers after the call returns with (propertyName, ""). The playback constants are the
// names pages compare media states with (their values weren't measured; owe never sends a state).
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
           "  define('wallpaperRequestRandomFileForProperty', function(propertyName, callback){"
           "    if (typeof callback !== 'function') return;"
           "    setTimeout(function(){ callback(propertyName, ''); }, 0);"
           "  });"
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

String BuildPropertiesPatchSnippet(const owe::Json& properties) {
    auto snippet =
        "(function(){"
        "  if (typeof window.wallpaperPropertyListener !== 'object') return;"
        "  if (typeof window.wallpaperPropertyListener.applyUserProperties !== 'function') return;"
        "  try {"
        "    window.wallpaperPropertyListener.applyUserProperties("_Str;
    snippet.push_str((properties.is_object() ? owe::DumpString(properties) : "{}"_Str).as_str());
    snippet.push_str(");"
                     "  } catch (e) {"
                     "    console.error('weweb: applyUserProperties patch threw:', e);"
                     "  }"
                     "})();"_str);
    return snippet;
}

namespace
{
// Wallpaper Engine's own properties as it hands them to every web page (web-property-dump-empty)
ref<str> OwnPropertiesJson() {
    return R"({"alignmentfliph":{"icon":"fa-exchange","order":-12,"text":"ui_browse_properties_alignment_flip_horizontally","type":"bool","value":false},)"
           R"("schemecolor":{"icon":"fa-palette","order":-20,"text":"ui_browse_properties_scheme_color","type":"color","value":"0.5 0.5 0.5"},)"
           R"("wec_brs":{"condition":"wec_e.value","icon":"fa-sun","max":100,"min":0,"order":-7,"text":"ui_browse_properties_brightness","type":"slider","value":50},)"
           R"("wec_con":{"condition":"wec_e.value","icon":"fa-adjust","max":100,"min":0,"order":-6,"text":"ui_browse_properties_contrast","type":"slider","value":50},)"
           R"("wec_e":{"order":-8,"text":"ui_browse_properties_show_color_options","type":"bool","value":false},)"
           R"("wec_hue":{"condition":"wec_e.value","icon":"fa-palette","max":100,"min":0,"order":-4,"text":"ui_browse_properties_hue_shift","type":"slider","value":50},)"
           R"("wec_sa":{"condition":"wec_e.value","icon":"fa-tint","max":100,"min":0,"order":-5,"text":"ui_browse_properties_saturation","type":"slider","value":50}})"_str;
}

// a property's value: {"value": x} or x itself
const owe::Json& PropertyValue(const owe::Json& property) {
    if (auto value = property.get("value"_str); value.is_some()) return **value;
    return property;
}

Option<double> NumberOf(const owe::Json& value) {
    if (auto number = value.as_f64(); number.is_some()) return Some(number->to_primitive());
    if (auto boolean = value.as_bool(); boolean.is_some()) return Some(*boolean ? 1.0 : 0.0);
    return None();
}

// a number as CSS gets it from Wallpaper Engine: at most 4 decimals, no trailing zeros (36, 1.3,
// -180)
String CssNumber(double value) {
    long long scaled = static_cast<long long>(value * 10000.0 + (value < 0 ? -0.5 : 0.5));
    auto      result = String {};
    if (scaled < 0) {
        result.push_ascii('-');
        scaled = -scaled;
    }
    Formatter formatter(result);
    formatter.write_fmt(Arguments::make("{}", i64(scaled / 10000)));
    auto fraction = scaled % 10000;
    if (fraction != 0) {
        char digits[5] = { char('0' + fraction / 1000),
                           char('0' + fraction / 100 % 10),
                           char('0' + fraction / 10 % 10),
                           char('0' + fraction % 10),
                           0 };
        int  length    = 4;
        while (digits[length - 1] == '0') --length;
        result.push_ascii('.');
        for (int i = 0; i < length; ++i) result.push_ascii(digits[i]);
    }
    return result;
}
} // namespace

bool OwnProperties::Apply(ref<str> key, const owe::Json& property) {
    const auto& value = PropertyValue(property);
    auto        flag  = [&] {
        if (auto boolean = value.as_bool(); boolean.is_some()) return *boolean;
        return NumberOf(value).unwrap_or(0.0) != 0.0;
    };
    auto number = [&](double& slot) {
        if (auto parsed = NumberOf(value); parsed.is_some()) slot = *parsed;
    };
    if (key == "alignmentfliph"_str) {
        flip = flag();
    } else if (key == "wec_e"_str) {
        colour = flag();
    } else if (key == "wec_brs"_str) {
        number(brightness);
    } else if (key == "wec_con"_str) {
        number(contrast);
    } else if (key == "wec_sa"_str) {
        number(saturation);
    } else if (key == "wec_hue"_str) {
        number(hue);
    } else {
        return false;
    }
    return true;
}

bool OwnProperties::Active() const { return flip || colour; }

String OwnProperties::Filter() const {
    if (! colour) return String {};
    auto filter = "contrast("_Str;
    filter.push_str(CssNumber(contrast / 50.0).as_str());
    filter.push_str(") brightness("_str);
    filter.push_str(CssNumber(brightness / 50.0).as_str());
    filter.push_str(") saturate("_str);
    filter.push_str(CssNumber(saturation / 50.0).as_str());
    filter.push_str(") hue-rotate("_str);
    filter.push_str(CssNumber((hue - 50.0) * 360.0 / 100.0).as_str());
    filter.push_str("deg)"_str);
    return filter;
}

String OwnProperties::StyleSnippet() const {
    auto snippet = "(function(){"
                   "  var e = document.documentElement; if (!e) return;"
                   "  var s = e.style;"
                   "  s.filter = '"_Str;
    snippet.push_str(Filter().as_str());
    snippet.push_str("';"
                     "  s.transform = '"_str);
    snippet.push_str(flip ? "scaleX(-1)"_str : ""_str);
    snippet.push_str("';"
                     "  s.overflowY = '"_str);
    snippet.push_str(flip ? "hidden"_str : ""_str);
    snippet.push_str("';"
                     "  if (e.getAttribute('style') === '') e.removeAttribute('style');"
                     "})();"_str);
    return snippet;
}

owe::Json WithWallpaperEngineProperties(const owe::Json& props) {
    auto result  = owe::ParseJson(OwnPropertiesJson()).unwrap();
    auto object  = result.as_object_mut().unwrap();
    bool has_own = false;
    if (auto properties = props.as_object(); properties.is_some()) {
        (*properties)->iter().for_each([&](auto entry) {
            auto [key, property] = entry;
            // Wallpaper Engine's own: its definitions stay (a project.json can't set them)
            if (IsWallpaperEnginesOwnProperty(key->as_str())) return;
            auto copy   = property->clone();
            auto fields = copy.as_object_mut();
            if (key->as_str() == "schemecolor"_str) {
                // the wallpaper's own scheme colour, where Wallpaper Engine puts it
                if (fields.is_some()) {
                    (*fields)->insert("order"_Str, rstd::into<owe::Json>(i64(-20)));
                    (*fields)->insert("icon"_Str, rstd::into<owe::Json>("fa-palette"_str));
                }
            } else {
                has_own = true;
                // after Wallpaper Engine's own: orders moved up by 100, a missing one null
                if (fields.is_some()) {
                    auto order = (*fields)->get("order"_str);
                    auto moved = owe::Json::Null();
                    if (order.is_some()) {
                        if (auto whole = (*order)->as_i64(); whole.is_some())
                            moved = rstd::into<owe::Json>(i64(whole->to_primitive() + 100));
                        else if (auto number = (*order)->as_f64(); number.is_some())
                            moved = rstd::into<owe::Json>(f64(number->to_primitive() + 100.0));
                    }
                    (*fields)->insert("order"_Str, rstd::move(moved));
                }
            }
            object->insert(key->clone(), rstd::move(copy));
        });
    }
    if (has_own) {
        auto divider = owe::ParseJson(R"({"order":0,"type":"divider"})"_str).unwrap();
        object->insert("_d0"_Str, rstd::move(divider));
    }
    return result;
}

owe::Json AsPropertyValues(const owe::Json& values) {
    auto result = rstd::json::Map::make();
    if (auto object = values.as_object(); object.is_some()) {
        (*object)->iter().for_each([&](auto entry) {
            auto [key, value] = entry;
            if (value->get("value"_str).is_some()) {
                result.insert(key->clone(), value->clone());
                return;
            }
            auto wrapped = rstd::json::Map::make();
            wrapped.insert("value"_Str, value->clone());
            result.insert(key->clone(), owe::Json::Object(rstd::move(wrapped)));
        });
    }
    return owe::Json::Object(rstd::move(result));
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
String BuildPausedSnippet(bool paused) {
    String snippet = "(function(){"
                     "  var paused = "_Str;
    snippet.push_str(paused ? "true;"_str : "false;"_str);
    snippet.push_str(
        "  var listener = window.wallpaperPropertyListener;"
        "  if (typeof listener === 'object' && listener && typeof listener.setPaused === 'function') {"
        "    try { listener.setPaused(paused); }"
        "    catch (e) { console.error('weweb: setPaused threw:', e); }"
        "  }"
        "  var root = document.documentElement;"
        "  if (!root) return;"
        "  root.classList.toggle('wpxPausePseudoAnimationAll', paused);"
        "  var all = document.querySelectorAll('*');"
        "  root.style.animationPlayState = paused ? 'paused' : '';"
        "  for (var i = 0; i < all.length; i++) all[i].style.animationPlayState = paused ? 'paused' : '';"
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

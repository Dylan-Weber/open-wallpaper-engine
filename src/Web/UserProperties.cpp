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

void InjectUserProperties(CefRefPtr<CefBrowser> browser, const owe::Json& props) {
    if (! browser) return;
    auto frame = browser->GetMainFrame();
    if (! frame) return;
    frame->ExecuteJavaScript(to_string(BuildPropertyListenerSnippet(props).as_str()),
                             "weweb://internal/inject_user_properties.js",
                             0);
}

} // namespace weweb

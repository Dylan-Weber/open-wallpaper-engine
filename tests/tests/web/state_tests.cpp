module;
#include <rstd/test/gtest.hpp>

module weweb;

import rstd;
import :cef;
import :cef_internal;
import :browser_host;

using namespace rstd::prelude;
using namespace rstd::literals;
using namespace weweb;

namespace
{
struct ReleaseProbe {
    int* count;
    explicit ReleaseProbe(int* value): count(value) {}
    ~ReleaseProbe() { ++*count; }
};
} // namespace

TEST(WebState, CpuFrameBorrowAndReentrantClear) {
    CefRefPtr<OsrRenderHandler> handler  = new OsrRenderHandler;
    int                         released = 0;
    int                         calls    = 0;
    rstd::array<rstd::u8, 8>    pixels {};
    auto                        owner = Box<ReleaseProbe>::make(&released);
    handler->SetCpuPaintCallback(
        Some(CpuPaintCallback::make([&, owner = rstd::move(owner)](const CpuPaintFrame& frame) {
            ++calls;
            EXPECT_EQ(frame.buffer, pixels.data());
            EXPECT_EQ(frame.width, 2);
            EXPECT_EQ(frame.height, 1);
            EXPECT_EQ(frame.row_stride, 8u);
            handler->SetCpuPaintCallback(None());
            handler->SetViewSize(32, 16);
            EXPECT_EQ(released, 0);
        })));
    handler->OnPaint(nullptr, PET_POPUP, {}, pixels.data(), 2, 1);
    handler->OnPaint(nullptr, PET_VIEW, {}, nullptr, 2, 1);
    handler->OnPaint(nullptr, PET_VIEW, {}, pixels.data(), 0, 1);
    EXPECT_EQ(calls, 0);
    handler->OnPaint(nullptr, PET_VIEW, {}, pixels.data(), 2, 1);
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(released, 1);
    handler->OnPaint(nullptr, PET_VIEW, {}, pixels.data(), 2, 1);
    EXPECT_EQ(calls, 1);
    CefRect rect;
    handler->GetViewRect(nullptr, rect);
    EXPECT_EQ(rect.width, 32);
    EXPECT_EQ(rect.height, 16);
}

TEST(WebState, CallbackReplacementReleasesOutsideLock) {
    CefRefPtr<OsrRenderHandler> handler = new OsrRenderHandler;
    struct ResizeOnRelease {
        OsrRenderHandler* handler;
        explicit ResizeOnRelease(OsrRenderHandler* value): handler(value) {}
        ~ResizeOnRelease() { handler->SetViewSize(64, 48); }
    };
    handler->SetCpuPaintCallback(Some(CpuPaintCallback::make(
        [owner = Box<ResizeOnRelease>::make(handler.get())](const CpuPaintFrame&) {
        })));
    handler->SetCpuPaintCallback(Some(CpuPaintCallback::make([](const CpuPaintFrame&) {
    })));
    CefRect rect;
    handler->GetViewRect(nullptr, rect);
    EXPECT_EQ(rect.width, 64);
    EXPECT_EQ(rect.height, 48);
}

TEST(WebState, ViewStateRejectsInvalidValues) {
    CefRefPtr<OsrRenderHandler> handler = new OsrRenderHandler;
    handler->SetViewSize(40, 30);
    handler->SetViewSize(0, 20);
    handler->SetDeviceScaleFactor(2.0f);
    handler->SetDeviceScaleFactor(-1.0f);
    handler->SetDeviceScaleFactor(__builtin_nanf(""));
    CefScreenInfo info;
    EXPECT_TRUE(handler->GetScreenInfo(nullptr, info));
    EXPECT_EQ(info.rect.width, 40);
    EXPECT_EQ(info.rect.height, 30);
    EXPECT_EQ(info.device_scale_factor, 2.0f);
}

TEST(WebState, AudioDemandCanClearItsOwnCallback) {
    CefRefPtr<OsrRenderHandler> render   = new OsrRenderHandler;
    CefRefPtr<ClientHandler>    client   = new ClientHandler(owe::Json::Null(), render, false);
    int                         released = 0;
    int                         calls    = 0;
    client->SetAudioDemandCallback(Some(
        AudioDemandCallback::make([&, owner = Box<ReleaseProbe>::make(&released)](bool active) {
            ++calls;
            EXPECT_FALSE(active);
            client->SetAudioDemandCallback(None());
            EXPECT_EQ(released, 0);
        })));
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(released, 1);
    client->RequestClose();
    EXPECT_TRUE(client->IsClosing());
    EXPECT_FALSE(client->IsClosed());
}

TEST(WebState, CloseWhileCreatingStopsCallbacksWithoutCompletingCreation) {
    CefRefPtr<OsrRenderHandler> render       = new OsrRenderHandler;
    CefRefPtr<ClientHandler>    client       = new ClientHandler(owe::Json::Null(), render, false);
    int                         paints       = 0;
    int                         demand_calls = 0;
    render->SetCpuPaintCallback(Some(CpuPaintCallback::make([&](const CpuPaintFrame&) {
        ++paints;
    })));
    client->SetAudioDemandCallback(Some(AudioDemandCallback::make([&](bool active) {
        EXPECT_FALSE(active);
        ++demand_calls;
    })));
    EXPECT_FALSE(client->IsClosing());
    EXPECT_FALSE(client->IsClosed());
    client->RequestClose();
    client->RequestClose();
    client->OnBeforePopupAborted(nullptr, 1);
    array<u8, 4> pixels {};
    render->OnPaint(nullptr, PET_VIEW, {}, pixels.data(), 1, 1);
    EXPECT_TRUE(client->IsClosing());
    EXPECT_FALSE(client->IsClosed());
    EXPECT_FALSE(client->GetBrowser());
    EXPECT_EQ(paints, 0);
    EXPECT_EQ(demand_calls, 2);
}

TEST(WebState, HostOwnsCallbacksBeforeInitialization) {
    int released = 0;
    {
        BrowserHost host;
        host.SetCpuPaintCallback(
            [owner = Box<ReleaseProbe>::make(&released)](const CpuPaintFrame&) {
            });
        host.SetCpuPaintCallback(None());
        EXPECT_EQ(released, 1);
        EXPECT_FALSE(host.ShouldExit());
        host.RequestClose();
        EXPECT_TRUE(host.IsClosing());
        EXPECT_TRUE(host.ShouldExit());
        EXPECT_TRUE(host.Shutdown());
        EXPECT_TRUE(host.Shutdown());
    }
    EXPECT_EQ(released, 1);
}

// Wallpaper Engine keeps one audio listener: a page that registers again replaces the first
// (measured with web-audio-listeners: the first stopped being called), registering null changes
// nothing, and there is no wallpaperRemoveAudioListener
TEST(WebState, AudioApiKeepsOneListener) {
    auto snippet = BuildAudioApiSnippet();
    auto text    = snippet.as_str();
    EXPECT_TRUE(text.contains("window.wallpaperRegisterAudioListener = function(cb){"_str));
    EXPECT_TRUE(text.contains("if (typeof cb !== 'function') return;"_str));
    EXPECT_TRUE(text.contains("listener = cb;"_str));
    EXPECT_FALSE(text.contains("listeners"_str));
    EXPECT_FALSE(text.contains("wallpaperRemoveAudioListener"_str));
    EXPECT_TRUE(text.contains("window.__weweb_pushAudio = function(arr){"_str));
}

TEST(WebState, AudioSnippetPreservesPayload) {
    rstd::array<float, 3> data { 0.0f, 0.5f, -1.0f };
    EXPECT_EQ(
        BuildAudioResponseSnippet(data.as_slice()).as_str(),
        "(function(){if(!window.__weweb_pushAudio)return;window.__weweb_pushAudio([0.00000000e0,5.00000000e-1,-1.00000000e0]);})();"_str);
}

TEST(WebState, AudioSnippetPreservesFloatPrecisionAndSpecialValues) {
    array<float, 1> subnormal { f32::from_bits(u32(1)).to_primitive() };
    EXPECT_EQ(
        BuildAudioResponseSnippet(subnormal.as_slice()).as_str(),
        "(function(){if(!window.__weweb_pushAudio)return;window.__weweb_pushAudio([1.40129846e-45]);})();"_str);
    array<float, 7> data { 0.1f,
                           -0.0f,
                           f32::MIN_POSITIVE.to_primitive(),
                           f32::MAX.to_primitive(),
                           f32::NAN_.to_primitive(),
                           f32::INFINITY_.to_primitive(),
                           (-f32::INFINITY_).to_primitive() };
    EXPECT_EQ(
        BuildAudioResponseSnippet(data.as_slice()).as_str(),
        "(function(){if(!window.__weweb_pushAudio)return;window.__weweb_pushAudio([1.00000001e-1,-0.00000000e0,1.17549435e-38,3.40282347e38,NaN,Infinity,-Infinity]);})();"_str);
    EXPECT_EQ(
        BuildAudioResponseSnippet(slice<float>()).as_str(),
        "(function(){if(!window.__weweb_pushAudio)return;window.__weweb_pushAudio([]);})();"_str);
}

TEST(WebState, PropertySnippetEscapesKey) {
    auto snippet = BuildPropertyPatchSnippet("a\"b"_str, owe::Json::Bool(true));
    EXPECT_TRUE(snippet.as_str().contains("{\"a\\\"b\":true}"_str));
    auto empty = BuildPropertyListenerSnippet(owe::Json::Null());
    EXPECT_TRUE(empty.as_str().contains("applyUserProperties({}"_str));
}

// Wallpaper Engine calls a page's audio listeners only when project.json's
// general.supportsaudioprocessing is true (VM, DevTools on its CEF, 2026-11-08: 30 calls a second
// with it, none without)
TEST(WebState, AudioDemandIsRefusedWithoutAudioProcessing) {
    CefRefPtr<OsrRenderHandler> render = new OsrRenderHandler;
    CefRefPtr<ClientHandler>    client =
        new ClientHandler(owe::Json::Null(), render, false, { .audio_allowed = false });
    int  calls = 0;
    bool last  = true;
    client->SetAudioDemandCallback(Some(AudioDemandCallback::make([&](bool active) {
        ++calls;
        last = active;
    })));
    auto message = CefProcessMessage::Create("weweb.audio-demand");
    message->GetArgumentList()->SetInt(0, 0);
    message->GetArgumentList()->SetBool(1, true);
    EXPECT_TRUE(client->OnProcessMessageReceived(nullptr, nullptr, PID_RENDERER, message));
    EXPECT_FALSE(last);
    EXPECT_EQ(calls, 1);
    client->RequestClose();
}

TEST(WebState, AudioDemandPassesWithAudioProcessing) {
    CefRefPtr<OsrRenderHandler> render = new OsrRenderHandler;
    CefRefPtr<ClientHandler>    client =
        new ClientHandler(owe::Json::Null(), render, false, { .audio_allowed = true });
    bool last = false;
    client->SetAudioDemandCallback(Some(AudioDemandCallback::make([&](bool active) {
        last = active;
    })));
    auto message = CefProcessMessage::Create("weweb.audio-demand");
    message->GetArgumentList()->SetInt(0, 0);
    message->GetArgumentList()->SetBool(1, true);
    EXPECT_TRUE(client->OnProcessMessageReceived(nullptr, nullptr, PID_RENDERER, message));
    EXPECT_TRUE(last);
    client->RequestClose();
}

// Wallpaper Engine hands a page applyGeneralProperties({fps}) with its frame rate setting (15 in
// the VM)
TEST(WebState, GeneralPropertiesSnippetGivesTheFrameRate) {
    auto snippet = BuildGeneralPropertiesSnippet(15);
    EXPECT_TRUE(snippet.as_str().contains("\"fps\":15,"_str));
    EXPECT_TRUE(snippet.as_str().contains("typeof window.wallpaperPropertyListener"_str));
}

// The page API Wallpaper Engine defines on window besides wallpaperRegisterAudioListener (read over
// DevTools in its CEF, VM 2026-11-08): owe defines each with the same type, so a page that calls
// one unguarded doesn't throw.
TEST(WebState, PageApiSnippetDefinesWallpaperEnginesNames) {
    auto               snippet = BuildPageApiSnippet();
    array<ref<str>, 9> names { "wallpaperRegisterMediaPropertiesListener"_str,
                               "wallpaperRegisterMediaThumbnailListener"_str,
                               "wallpaperRegisterMediaPlaybackListener"_str,
                               "wallpaperRegisterMediaTimelineListener"_str,
                               "wallpaperRegisterMediaStatusListener"_str,
                               "wallpaperGetUtilities"_str,
                               "wallpaperRequestRandomFileForProperty"_str,
                               "wallpaperOnVideoEnded"_str,
                               "wallpaperRequestTakeScreenshotResponse"_str };
    for (auto name : names) {
        auto definition = String::make("define('"_str);
        definition.push_str(name);
        definition.push_str("', function"_str);
        EXPECT_TRUE(snippet.as_str().contains(definition.as_str()));
    }
    EXPECT_TRUE(snippet.as_str().contains("define('wallpaperMediaIntegration', {"_str));
    // a page's own definition wins
    EXPECT_TRUE(snippet.as_str().contains("if (typeof window[name] === 'undefined')"_str));
}

// Wallpaper Engine tells the page its frame rate setting but draws it at the display's rate:
// requestAnimationFrame ran 63-64 times a second in the VM with the setting at 15 and at 30
TEST(WebState, PageIsToldTheSettingButDrawnAtItsOwnRate) {
    BrowserHost::OpenOptions open;
    open.general_fps = 15;
    EXPECT_EQ(open.frame_rate, 60);
    EXPECT_EQ(ToldFrameRate(open), 15);
    open.general_fps = 0;
    open.frame_rate  = 30;
    EXPECT_EQ(ToldFrameRate(open), 30);
}

// wallpaperRequestRandomFileForProperty with no folder chosen (or a name that is no property): Wallpaper
// Engine calls back after the call returns with (propertyName, "") (web-random-file, VM 2026-11-09)
TEST(WebState, RandomFileCallsBackWithNoFile) {
    auto snippet = BuildPageApiSnippet();
    auto text    = snippet.as_str();
    EXPECT_TRUE(text.contains(
        "define('wallpaperRequestRandomFileForProperty', function(propertyName, callback){"_str));
    EXPECT_TRUE(text.contains("if (typeof callback !== 'function') return;"_str));
    EXPECT_TRUE(text.contains("setTimeout(function(){ callback(propertyName, ''); }, 0);"_str));
}

// Wallpaper Engine adds its own properties to what applyUserProperties hands a web page: its flip
// and colour options and a scheme colour, a divider before the wallpaper's own properties, whose
// orders move up by 100 (a missing order becomes null). Expected: web-property-dump-noorder as read
// over DevTools in the VM (2026-11-09)
TEST(WebState, WallpaperEnginesOwnPropertiesAreAdded) {
    auto props =
        owe::ParseJson(
            R"({"schemecolor":{"text":"ui_browse_properties_scheme_color","type":"color","value":"1 0 0"},"plain":{"text":"plain","type":"bool","value":true},"negative":{"order":-3,"text":"negative","type":"bool","value":false}})"_str)
            .unwrap();
    auto expected =
        owe::ParseJson(
            R"({"_d0":{"order":0,"type":"divider"},"alignmentfliph":{"icon":"fa-exchange","order":-12,"text":"ui_browse_properties_alignment_flip_horizontally","type":"bool","value":false},"negative":{"order":97,"text":"negative","type":"bool","value":false},"plain":{"order":null,"text":"plain","type":"bool","value":true},"schemecolor":{"icon":"fa-palette","order":-20,"text":"ui_browse_properties_scheme_color","type":"color","value":"1 0 0"},"wec_brs":{"condition":"wec_e.value","icon":"fa-sun","max":100,"min":0,"order":-7,"text":"ui_browse_properties_brightness","type":"slider","value":50},"wec_con":{"condition":"wec_e.value","icon":"fa-adjust","max":100,"min":0,"order":-6,"text":"ui_browse_properties_contrast","type":"slider","value":50},"wec_e":{"order":-8,"text":"ui_browse_properties_show_color_options","type":"bool","value":false},"wec_hue":{"condition":"wec_e.value","icon":"fa-palette","max":100,"min":0,"order":-4,"text":"ui_browse_properties_hue_shift","type":"slider","value":50},"wec_sa":{"condition":"wec_e.value","icon":"fa-tint","max":100,"min":0,"order":-5,"text":"ui_browse_properties_saturation","type":"slider","value":50}})"_str)
            .unwrap();
    auto result = WithWallpaperEngineProperties(props);
    EXPECT_EQ(owe::DumpString(result).as_str(), owe::DumpString(expected).as_str());
}

// With no properties of its own: no divider, and a grey scheme colour (web-property-dump-empty)
TEST(WebState, WallpaperEnginesOwnPropertiesWithoutTheWallpapers) {
    auto result = WithWallpaperEngineProperties(owe::ParseJson("{}"_str).unwrap());
    EXPECT_TRUE(result.get("_d0"_str).is_none());
    auto scheme = result.pointer("/schemecolor/value"_str);
    ASSERT_TRUE(scheme.is_some());
    EXPECT_EQ((*scheme)->as_str(), Some("0.5 0.5 0.5"_str));
    EXPECT_TRUE(result.pointer("/wec_e/value"_str).is_some());
}

// A project.json can't set Wallpaper Engine's own properties: the page gets its own definitions
// (web-color-adjust-brs-100 with a project.json wec_brs of 100: the page saw 50), and a user's
// saved values for them are applied after load (OwnPropertiesOf)
TEST(WebState, ProjectCannotSetWallpaperEnginesOwnProperties) {
    auto props =
        owe::ParseJson(
            R"({"wec_brs":{"type":"slider","value":100,"min":0,"max":100,"order":0},"mine":{"type":"bool","value":true,"order":1}})"_str)
            .unwrap();
    auto result = WithWallpaperEngineProperties(props);
    auto brs    = result.pointer("/wec_brs/value"_str);
    ASSERT_TRUE(brs.is_some());
    EXPECT_EQ(owe::DumpString(**brs).as_str(), "50"_str);
    EXPECT_TRUE(result.pointer("/wec_brs/condition"_str).is_some());
    EXPECT_TRUE(IsWallpaperEnginesOwnProperty("wec_hue"_str));
    EXPECT_TRUE(IsWallpaperEnginesOwnProperty("alignmentfliph"_str));
    EXPECT_FALSE(IsWallpaperEnginesOwnProperty("schemecolor"_str));
    auto own = OwnPropertiesOf(*props.as_object().unwrap());
    EXPECT_TRUE(own.get("wec_brs"_str).is_some());
    EXPECT_TRUE(own.get("mine"_str).is_none());
}

// Values set with applyProperties reach the page as {"value": ...}, as project.json's do
TEST(WebState, AppliedPropertiesAreHandedAsValues) {
    auto applied =
        owe::ParseJson(R"({"wec_e":true,"wec_brs":65,"x":{"value":"kept"}})"_str).unwrap();
    auto handed = AsPropertyValues(applied);
    EXPECT_EQ(owe::DumpString(handed).as_str(),
              owe::DumpString(
                  owe::ParseJson(
                      R"({"wec_brs":{"value":65},"wec_e":{"value":true},"x":{"value":"kept"}})"_str)
                      .unwrap())
                  .as_str());
}

// Wallpaper Engine applies its colour options with a CSS filter on <html>: contrast, brightness and
// saturation the slider over 50, the hue (slider - 50) * 3.6 degrees (web-color-adjust-*, VM)
TEST(WebState, OwnColourOptionsAreACssFilter) {
    OwnProperties own;
    auto          value = [](auto v) {
        return owe::ParseJson(rstd::format("{{\"value\":{}}}", v).as_str()).unwrap();
    };
    EXPECT_TRUE(own.Apply("wec_brs"_str, value(65)));
    EXPECT_EQ(own.Filter().as_str(), ""_str); // off until wec_e
    EXPECT_TRUE(own.Apply("wec_e"_str, value("true")));
    EXPECT_TRUE(own.Apply("wec_con"_str, value(35)));
    EXPECT_TRUE(own.Apply("wec_sa"_str, value(70)));
    EXPECT_TRUE(own.Apply("wec_hue"_str, value(60)));
    EXPECT_EQ(own.Filter().as_str(),
              "contrast(0.7) brightness(1.3) saturate(1.4) hue-rotate(36deg)"_str);
    own.Apply("wec_hue"_str, value(0));
    own.Apply("wec_brs"_str, value(0));
    own.Apply("wec_con"_str, value(100));
    own.Apply("wec_sa"_str, value(0));
    EXPECT_EQ(own.Filter().as_str(),
              "contrast(2) brightness(0) saturate(0) hue-rotate(-180deg)"_str);
    EXPECT_FALSE(own.Apply("schemecolor"_str, value(1)));
}

// The flip is transform: scaleX(-1) and overflow-y: hidden on <html>, with or without the colour
// options; with neither, nothing is set
TEST(WebState, OwnFlipIsACssTransform) {
    OwnProperties own;
    EXPECT_FALSE(own.Active());
    own.Apply("alignmentfliph"_str, owe::ParseJson(R"({"value":true})"_str).unwrap());
    EXPECT_TRUE(own.Active());
    auto snippet = own.StyleSnippet();
    EXPECT_TRUE(snippet.as_str().contains("s.filter = '';"_str));
    EXPECT_TRUE(snippet.as_str().contains("s.transform = 'scaleX(-1)';"_str));
    EXPECT_TRUE(snippet.as_str().contains("s.overflowY = 'hidden';"_str));
}

// Wallpaper Engine hands applyGeneralProperties its whole general config, 60 keys (read in the VM, probe
// _scratch/web-general-dump); owe gives the same keys, its own frame rate and neutral values for the rest
// Wallpaper Engine hands applyGeneralProperties its whole general config, 60 keys (read in the VM,
// probe _scratch/web-general-dump); owe gives the same keys, its own frame rate and neutral values
// for the rest
TEST(WebState, GeneralPropertiesCarryTheWholeConfig) {
    auto expected =
        owe::ParseJson(
            R"({"adjustdwmcolormode":"disabled","anticrash":null,"apprules":[],"audioinputdevice":"default","audioinputthreshold":0,"audioinputvolume":50,"autostart":false,"autostartscheduler":false,"autostartx64":false,"browsetransition":{"transition":"none","transitiontime":1500},"cefcommandline":"","fps":30,"hasshownwelcomedialog":true,"highprecisiontimer":true,"iconopacity":100,"language":"en-us","location":{},"loglevel":"error","mediablocklist":null,"mediaintegration":true,"monitordetection":"managed","monitormap":{},"msaa":"none","overridelockscreen":false,"overridewallpaper":false,"pausevram":false,"playbackaudio":"run","playbackfocus":"run","playbackfullscreen":"run","playbackmaximized":"run","playbackonbattery":"run","playbacksleep":"run","plugindelay":0,"plugins":{"ledextensions":{"enabled":true}},"postprocessing":"enabled","preset":"medium","processpriority":"normal","reflection":true,"reloadaudio":true,"resolution":"full","safemode":false,"shadows":"medium","slideshowkiller":false,"uicmd":"","uieffects":null,"uihardwareacceleration":true,"uiquality":"high","uiskin":"dark","uiskinseasonal":true,"unpauseaero":true,"vdesktopenabled":false,"videoaudiooutput":true,"videoframework":"mfEngine","videohardwareacceleration":true,"videoloopmode":"default","videomfstutterhack":false,"videoreadmode":"fromdisk","volumetrics":"medium","webmframework":null,"windowupdaterate":1000})"_str)
            .unwrap();
    auto parsed = owe::ParseJson(BuildGeneralPropertiesJson(30).as_str());
    ASSERT_TRUE(parsed.is_ok());
    EXPECT_EQ(owe::DumpString(parsed.unwrap()).as_str(), owe::DumpString(expected).as_str());
    // this machine's own settings (monitor map, command line) aren't handed to the page
    auto other = owe::ParseJson(BuildGeneralPropertiesJson(15).as_str()).unwrap();
    auto fps   = other.pointer("/fps"_str);
    ASSERT_TRUE(fps.is_some());
    EXPECT_EQ(owe::DumpString(**fps).as_str(), "15"_str);
}

// Wallpaper Engine adds a style element to the page's head with these rules (read over DevTools)
TEST(WebState, PausePseudoAnimationsStyleIsInjectedOnce) {
    auto snippet = BuildPausePseudoAnimationsSnippet();
    auto text    = snippet.as_str();
    EXPECT_TRUE(text.contains(
        ".wpxPausePseudoAnimations:before{animation-play-state:paused !important}"_str));
    EXPECT_TRUE(text.contains(
        ".wpxPausePseudoAnimations:after{animation-play-state:paused !important}"_str));
    EXPECT_TRUE(
        text.contains(".wpxPausePseudoAnimationAll *{animation-play-state:paused !important}"_str));
    EXPECT_TRUE(text.contains(
        ".wpxPausePseudoAnimationAll *:before{animation-play-state:paused !important}"_str));
    EXPECT_TRUE(text.contains(
        ".wpxPausePseudoAnimationAll *:after{animation-play-state:paused !important}"_str));
    EXPECT_TRUE(snippet.as_str().contains("type=\"text/css\""_str) ||
                snippet.as_str().contains("'text/css'"_str));
}

// setPaused reaches the page, which then gets the class and an inline animation-play-state on every
// element
TEST(WebState, PausedSnippetCallsSetPausedAndPausesAnimations) {
    auto paused = BuildPausedSnippet(true);
    EXPECT_TRUE(paused.as_str().contains("var paused = true;"_str));
    EXPECT_TRUE(paused.as_str().contains("setPaused(paused)"_str));
    EXPECT_TRUE(paused.as_str().contains("wpxPausePseudoAnimationAll"_str));
    EXPECT_TRUE(paused.as_str().contains("paused"_str));
    auto playing = BuildPausedSnippet(false);
    EXPECT_TRUE(playing.as_str().contains("var paused = false;"_str));
}

// the browser stops the page's renderer when the page says it has run setPaused(true), as Wallpaper
// Engine does: no timer or animation frame runs and the page stays "visible" until resumed;
// resuming sends no acknowledgement
TEST(WebState, PausedSnippetAcknowledgesOnlyWhenPausing) {
    auto paused = BuildPausedSnippet(true);
    EXPECT_TRUE(paused.as_str().contains("window.__weweb_pauseAck()"_str));
    auto playing = BuildPausedSnippet(false);
    EXPECT_TRUE(playing.as_str().contains("window.__weweb_pauseAck()"_str));
    EXPECT_TRUE(playing.as_str().contains("if (paused && "_str));
}

TEST(WebManifest, LoadsOwnedStringsAndDefaults) {
    auto temporary = rstd::fs::TempDir::make("owe-web-manifest"_str).unwrap();
    auto root      = rstd::path::PathBuf::from(temporary.path());
    auto path      = root.as_path();
    auto content =
        R"({/*comment*/"type":"WeB","title":"\u4f60\u597d","file":"page.html","preview":"preview.jpg","general":{"properties":{"enabled":{"value":true}}}})"_str;
    ASSERT_TRUE(
        rstd::fs::write(root.join("project.json"_str).as_path(), content.as_bytes()).is_ok());
    auto manifest = LoadWebManifest(path);
    ASSERT_TRUE(manifest.is_some());
    EXPECT_EQ(manifest->title.as_str(), "\xe4\xbd\xa0\xe5\xa5\xbd"_str);
    EXPECT_EQ(manifest->entry_html.as_str(), "page.html"_str);
    ASSERT_TRUE(manifest->preview.is_some());
    EXPECT_EQ(manifest->preview->as_str(), "preview.jpg"_str);
    EXPECT_TRUE(manifest->user_props.get("enabled"_str).is_some());
    EXPECT_FALSE(manifest->supports_audio);
    ASSERT_TRUE(
        rstd::fs::write(root.join("project.json"_str).as_path(),
                        R"({"type":"web","title":0,"file":null,"preview":false})"_str.as_bytes())
            .is_ok());
    auto defaults = LoadWebManifest(path);
    ASSERT_TRUE(defaults.is_some());
    EXPECT_EQ(defaults->title.as_str(), "Wallpaper"_str);
    EXPECT_EQ(defaults->entry_html.as_str(), "index.html"_str);
    EXPECT_TRUE(defaults->preview.is_none());
    EXPECT_EQ(manifest->entry_html.as_str(), "page.html"_str);
}

TEST(WebManifest, RejectsInvalidDocuments) {
    auto temporary = rstd::fs::TempDir::make("owe-web-invalid"_str).unwrap();
    auto root      = rstd::path::PathBuf::from(temporary.path());
    auto path      = root.as_path();
    EXPECT_TRUE(LoadWebManifest(path).is_none());
    for (auto content : rstd::array<ref<str>, 6> { "{"_str,
                                                   "{}"_str,
                                                   R"({"type":1})"_str,
                                                   R"({"type":"scene"})"_str,
                                                   R"({"type":"\u4f60"})"_str,
                                                   R"({"type":"web\u0000"})"_str }) {
        ASSERT_TRUE(
            rstd::fs::write(root.join("project.json"_str).as_path(), content.as_bytes()).is_ok());
        EXPECT_TRUE(LoadWebManifest(path).is_none());
    }
}

TEST(WebManifest, ReadsNativePathBytes) {
    auto temporary = rstd::fs::TempDir::make("owe-web-path"_str).unwrap();
    auto root      = rstd::path::PathBuf::from(temporary.path());
    auto name      = Vec<u8>::from("web-"_str.as_bytes());
    name.push(u8(0xff));
    auto native    = rstd::ffi::OsString::from_encoded_bytes_unchecked(rstd::move(name));
    auto directory = root.join(rstd::ref<rstd::path::Path>(native.as_os_str()));
    ASSERT_TRUE(rstd::fs::create_dir(directory.as_path()).is_ok());
    ASSERT_TRUE(rstd::fs::write(directory.join("project.json"_str).as_path(),
                                R"({"type":"web"})"_str.as_bytes())
                    .is_ok());
    EXPECT_TRUE(LoadWebManifest(directory.as_path()).is_some());
}

#if ! __is_target_os(macos)
TEST(WebState, AcceleratedFrameBorrowsDescriptors) {
    CefRefPtr<OsrRenderHandler> handler = new OsrRenderHandler;
    int                         calls   = 0;
    handler->SetAcceleratedPaintCallback(
        Some(AcceleratedPaintCallback::make([&](const DmaBufFrame& frame) {
            ++calls;
            EXPECT_EQ(frame.plane_count, 1);
            EXPECT_EQ(frame.planes[0].fd, -1);
            EXPECT_EQ(frame.planes[0].stride, 64u);
            EXPECT_EQ(frame.coded_width, 16);
            EXPECT_EQ(frame.visible_width, 12);
            handler->SetAcceleratedPaintCallback(None());
        })));
    CefAcceleratedPaintInfo info {};
    info.plane_count              = 1;
    info.planes[0].fd             = -1;
    info.planes[0].stride         = 64;
    info.extra.coded_size.width   = 16;
    info.extra.visible_rect.width = 12;
    handler->OnAcceleratedPaint(nullptr, PET_POPUP, {}, info);
    EXPECT_EQ(calls, 0);
    handler->OnAcceleratedPaint(nullptr, PET_VIEW, {}, info);
    EXPECT_EQ(calls, 1);
    handler->OnAcceleratedPaint(nullptr, PET_VIEW, {}, info);
    EXPECT_EQ(calls, 1);
}
#endif

TEST(WebManifest, ReadsSupportsAudioProcessing) {
    auto temporary = rstd::fs::TempDir::make("owe-web-manifest-audio"_str).unwrap();
    auto root      = rstd::path::PathBuf::from(temporary.path());
    auto content   = R"({"type":"web","general":{"supportsaudioprocessing":true}})"_str;
    ASSERT_TRUE(
        rstd::fs::write(root.join("project.json"_str).as_path(), content.as_bytes()).is_ok());
    auto manifest = LoadWebManifest(root.as_path());
    ASSERT_TRUE(manifest.is_some());
    EXPECT_TRUE(manifest->supports_audio);
}

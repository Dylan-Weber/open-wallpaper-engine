#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

import vvk;
import rstd;
import rstd.bench;
import rstd.log;
import wavsen.audio;
import wescene.core;
import wescene.json;
import wescene.scene_wallpaper;
import wescene.utils;
import viewer.common;
import viewer.audio;
import viewer.stdin_control;
import wescene.capture_surface;

import viewer.glfw_vulkan;

using namespace viewer::glfw;

#if __is_target_os(macos)
extern "C" VkResult oweCreateGlfwCocoaSurface(GLFWwindow*, VkInstance, VkSurfaceKHR*, int, int);
extern "C" void     oweConfigureGlfwCocoaLayer(GLFWwindow*, int, int);
#endif

using namespace rstd::prelude;
using namespace rstd::literals;
using rstd::io::eprintln;
using rstd::io::println;
using rstd::sync::Arc;
using rstd::sync::atomic::Atomic;
using rstd::sync::atomic::Ordering;
using rstd::time::Duration;
using rstd::time::Instant;

// --capture's outcome, set by the render thread once the frame is written (or isn't).
struct CaptureState {
    Atomic<bool> done { false };
    Atomic<bool> ok { false };
};

// Writes tightly packed RGBA rows as a binary PPM (P6: a short header, then RGB rows).
bool WritePpm(const rstd::path::PathBuf& path, unsigned width, unsigned height, slice<u8> rgba) {
    const auto header = rstd::format("P6\n{} {}\n255\n", width, height);
    Vec<u8>    ppm;
    for (auto byte : header.as_str().as_bytes()) ppm.push(u8(byte));
    for (usize pixel {}; pixel * usize(4) < rgba.len(); ++pixel)
        for (usize channel {}; channel < usize(3); ++channel)
            ppm.push(u8(rgba[pixel * usize(4) + channel]));
    return rstd::fs::write(path.as_path(), ppm.as_slice()).is_ok();
}

struct UserData {
    owe::SceneWallpaper* psw { nullptr };
    bool                 mouse_position_locked { false };
};

extern "C" {
#if __is_target_os(macos)
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    // GLFW reports backing pixels here when the Cocoa Retina framebuffer hint
    // is enabled. Keep the CAMetalLayer drawable size synchronized on the
    // Cocoa main thread after a display/scale or window-size change.
    oweConfigureGlfwCocoaLayer(window, width, height);
}
#else
void framebuffer_size_callback(GLFWwindow*, int, int) {}
#endif

void mouse_button_callback(GLFWwindow* win, int button, int action, int /*mods*/) {
    UserData* data = static_cast<UserData*>(glfwGetWindowUserPointer(win));
    if (! data || ! data->psw) return;
    // GLFW button numbering (0=left, 1=right, 2=middle) matches WE.
    if (action == viewer::glfw::Press) data->psw->mouseButton(button, true);
    if (action == viewer::glfw::Release) data->psw->mouseButton(button, false);
}

void cursor_position_callback(GLFWwindow* win, double xpos, double ypos) {
    UserData* data = static_cast<UserData*>(glfwGetWindowUserPointer(win));
    if (! data || ! data->psw || data->mouse_position_locked) return;
    int width  = 0;
    int height = 0;
    glfwGetWindowSize(win, &width, &height);
    if (width <= 0 || height <= 0) return;
    data->psw->mouseInput(xpos / static_cast<double>(width), ypos / static_cast<double>(height));
}

void cursor_enter_callback(GLFWwindow* win, int entered) {
    UserData* data = static_cast<UserData*>(glfwGetWindowUserPointer(win));
    if (! data || ! data->psw || data->mouse_position_locked) return;
    data->psw->mouseEnter(entered != 0);
}
}

int main(int argc, char** argv) {
    static rstd::log::EnvLogger _logger;
    rstd::log::set_logger(_logger);
    rstd::log::set_max_level(_logger.filter());

    auto args                = viewer::ParseSceneViewerArgs(argc, argv);
    auto [w_width, w_height] = args.resolution;
    // --capture renders offscreen, without a window, and exits once the frame is written.
    const bool capture       = ! args.capture_output.is_empty();
    auto       capture_state = Arc<CaptureState>::make();

    auto load_bench = owe::CreateSceneLoadBench(args.load_bench_output.as_str());
    Option<rstd::bench::probe::ProbeRecorder> startup_recorder;
    if (load_bench) startup_recorder = Some((**load_bench).session().recorder());
    auto startup_span = owe::SceneLoadSpan({ startup_recorder ? &*startup_recorder : nullptr,
                                             load_bench ? &(**load_bench).ids() : nullptr },
                                           &owe::SceneLoadProbeIds::viewer_startup);

    viewer::InitGlfwPlatformHint(/*force_x11=*/false);
#if __is_target_os(macos)
    auto loader_result = vvk::VulkanLoader::Open();
    if (loader_result.is_err()) {
        eprintln("Failed to load Vulkan");
        return -1;
    }
    auto vulkan_loader = loader_result.unwrap_unchecked();
    viewer::glfw::glfwInitVulkanLoader(vulkan_loader.global().vkGetInstanceProcAddr);
    if (! glfwInit()) {
        eprintln("Failed to initialize GLFW");
        return -1;
    }
#else
    glfwInit();
#endif
    glfwWindowHint(viewer::glfw::ClientApi, viewer::glfw::NoApi);
#if __is_target_os(macos)
    // Keep the backing framebuffer in physical pixels. The window remains
    // sized in Cocoa points, while the CAMetalLayer/Vulkan swapchain use the
    // 2x (or display-specific) backing dimensions.
    glfwWindowHint(viewer::glfw::CocoaRetinaFramebuffer, viewer::glfw::TrueValue);
#endif
    // Bulk-scan path: WP_HEADLESS=1 hides the window so a scan loop over
    // hundreds of pkgs doesn't spam the desktop. Compile/render still
    // runs against the offscreen surface — stderr captures shader errors.
    if (auto hl = rstd::env::var_os("WP_HEADLESS"_str);
        hl && ! hl->is_empty() && hl->as_os_str().as_encoded_bytes()[usize()] == u8('1')) {
        glfwWindowHint(viewer::glfw::Visible, viewer::glfw::FalseValue);
    }
    GLFWwindow* window =
        capture ? nullptr : glfwCreateWindow(w_width, w_height, "WP", nullptr, nullptr);
#if __is_target_os(macos)
    if (! capture && window == nullptr) {
        eprintln("Failed to create GLFW window");
        glfwTerminate();
        return -1;
    }
#endif
    int render_width  = w_width;
    int render_height = w_height;
#if __is_target_os(macos)
    if (! capture) glfwGetFramebufferSize(window, &render_width, &render_height);
    if (render_width <= 0) render_width = w_width;
    if (render_height <= 0) render_height = w_height;
#endif
    UserData data;

    owe::RenderInitInfo info;
    info.enable_valid_layer = args.enable_valid_layer;
    if (args.async_pipelines)
        info.pipeline_preparation = vrento::vulkan::PipelinePreparationMode::Deferred;
    info.width        = static_cast<rstd::uint16_t>(render_width);
    info.height       = static_cast<rstd::uint16_t>(render_height);
    info.msaa_samples = args.msaa_samples.to_primitive();

    auto& sf_info = info.surface_info;
    if (capture) {
        info.offscreen = true;
        sf_info.createSurfaceOp =
            Some(owe::CreateSurfaceCallback::make([](VkInstance, VkSurfaceKHR*) -> VkResult {
                return VK_SUCCESS;
            }));
        const auto width          = static_cast<unsigned>(render_width);
        const auto height         = static_cast<unsigned>(render_height);
        info.ex_swapchain_factory = Some(owe::MakeCaptureSurfaceFactory(
            owe::CaptureRequest { .frame = args.capture_frame, .width = width, .height = height },
            owe::CaptureCallback::make([state = capture_state.clone(),
                                        path =
                                            rstd::path::PathBuf::from(args.capture_output.as_str()),
                                        width,
                                        height](Option<Vec<u8>> pixels) {
                bool ok = pixels.is_some() && WritePpm(path, width, height, pixels->as_slice());
                if (! ok)
                    eprintln("--capture: cannot write '{}'", path.as_path().as_os_str().display());
                state->ok.store(ok, Ordering::Release);
                state->done.store(true, Ordering::Release);
            })));
    } else {
        uint32_t glfwExtCount = 0;
        auto     exts         = glfwGetRequiredInstanceExtensions(&glfwExtCount);
#if __is_target_os(macos)
        if (exts == nullptr || glfwExtCount == 0) {
            eprintln("GLFW did not provide Vulkan instance extensions");
            glfwDestroyWindow(window);
            glfwTerminate();
            return -1;
        }
#endif
        for (uint32_t i = 0; i < glfwExtCount; i++) {
            sf_info.instanceExts.push(
                String::make(rstd::ffi::CStr::from_ptr(exts[i]).to_str().unwrap()));
        }
#if __is_target_os(macos)
        sf_info.createSurfaceOp = Some(owe::CreateSurfaceCallback::make(
            [window, render_width, render_height](VkInstance inst, VkSurfaceKHR* surface) {
                return oweCreateGlfwCocoaSurface(
                    window, inst, surface, render_width, render_height);
            }));
#else
        sf_info.createSurfaceOp =
            Some(owe::CreateSurfaceCallback::make([window](VkInstance inst, VkSurfaceKHR* surface) {
                return viewer::glfw::glfwCreateWindowSurface(inst, window, nullptr, surface);
            }));
#endif
    }

#if ! __is_target_os(macos)
    if (! capture && window == nullptr) {
        println("Failed to create GLFW window");
        glfwTerminate();
        return -1;
    }
#endif

    auto  texture_memory = args.texture_system ? owe::TextureMemorySession::System()
                                               : owe::TextureMemorySession::VirtualMemory();
    auto* psw            = new owe::SceneWallpaper(texture_memory.options());
    data.psw             = psw;

    Atomic<bool> audio_response_demand { false };
    psw->setAudioResponseDemandCallback([&audio_response_demand](bool active) {
        audio_response_demand.store(active, Ordering::Release);
    });

    psw->init();

    owe::SceneWallpaperConfig config;
    config.assets_dir      = rstd::move(args.assets_dir);
    config.source_pkg_path = rstd::move(args.scene_path);
    config.graphviz        = args.graphviz;
    config.load_bench      = load_bench.clone();
    config.fps             = static_cast<uint32_t>(args.fps.to_primitive());
    if (args.random_seed.is_some()) {
        config.random_seed = Some(*args.random_seed);
    }

    config.cache_dir = args.cache_path.is_empty() ? viewer::DefaultCacheDir("wescene-renderer"_str)
                                                  : rstd::move(args.cache_path);

    // Apply --user-properties FILE before the scene loads so the first
    // frame already reflects the user's edits. Mirrors the daemon path
    // (Init.user_properties): JSON object whose values can be strings,
    // numbers, or booleans.
    if (const auto& up_path = args.user_properties_path; ! up_path.is_empty()) {
        auto source = rstd::fs::read_to_string(up_path);
        if (source.is_err()) {
            eprintln("--user-properties: cannot open '{}'",
                     up_path.as_path().as_os_str().display());
            return 1;
        }
        auto parsed_result = owe::ParseJson(source->as_str(), { .allow_comments = true });
        if (parsed_result.is_err()) {
            auto error = parsed_result.unwrap_err();
            eprintln("--user-properties: '{}' is invalid JSON at line {} column {}",
                     up_path.as_path().as_os_str().display(),
                     error.line(),
                     error.column());
            return 1;
        }
        auto parsed = parsed_result.unwrap();
        if (! parsed.is_object()) {
            eprintln("--user-properties: '{}' is not a JSON object",
                     up_path.as_path().as_os_str().display());
            return 1;
        }
        auto object = parsed.as_object();
        (*object)->iter().for_each([&](auto entry) {
            auto [entry_key, entry_value] = entry;
            config.user_properties.insert(entry_key->clone(), entry_value->clone());
        });
    }

    // --audio-spectrum FILE: shaders and scripts see these bands every frame, not what is heard
    if (const auto& spectrum_path = args.audio_spectrum_path; ! spectrum_path.is_empty()) {
        auto source = rstd::fs::read_to_string(spectrum_path);
        if (source.is_err()) {
            eprintln("--audio-spectrum: cannot open '{}'",
                     spectrum_path.as_path().as_os_str().display());
            return 1;
        }
        auto spectrum = owe::scene_audio::ParseFixedSpectrum(source->as_str());
        if (spectrum.is_err()) {
            eprintln("--audio-spectrum: '{}': {}",
                     spectrum_path.as_path().as_os_str().display(),
                     spectrum.unwrap_err());
            return 1;
        }
        config.fixed_audio = Some(spectrum.unwrap());
    }

    (void)startup_span.finish();
    if (startup_recorder) {
        auto batch = startup_recorder->drain();
        if (batch.is_ok())
            (**load_bench).add_preload_batch(rstd::move(batch).unwrap_unchecked());
        else
            eprintln("cannot drain viewer startup probes");
        startup_recorder = None();
    }
    load_bench = None();
    psw->configure(rstd::move(config));
    psw->initVulkan(rstd::move(info));

    if (window != nullptr) {
        glfwSetWindowUserPointer(window, &data);
        glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
        glfwSetMouseButtonCallback(window, mouse_button_callback);
        glfwSetCursorPosCallback(window, cursor_position_callback);
        glfwSetCursorEnterCallback(window, cursor_enter_callback);
    }

    auto locked_mouse          = viewer::ParseMousePosition(args.mouse_position.as_str());
    data.mouse_position_locked = locked_mouse.is_some();
    auto apply_locked_mouse    = [&]() {
        if (! locked_mouse) return;
        psw->mouseEnter(true);
        psw->mouseInput((*locked_mouse)[usize()], (*locked_mouse)[usize(1)]);
        int window_width  = 0;
        int window_height = 0;
        if (window == nullptr) return;
        glfwGetWindowSize(window, &window_width, &window_height);
        if (window_width > 0 && window_height > 0) {
            glfwSetCursorPos(window,
                             (*locked_mouse)[usize()] * window_width,
                             (*locked_mouse)[usize(1)] * window_height);
        }
    };
    apply_locked_mouse();

    viewer::StdinJsonControl stdin_control(args.stdin_json);
#if __is_target_os(macos)
    viewer::AudioCaptureWorker audio_capture;
#else
    wavsen::audio::AudioCapture audio_capture;
    auto                        next_audio_update = Instant::now();
#endif
    bool audio_ended  = true;
    auto update_audio = [&] {
#if __is_target_os(macos)
        const bool demanded = audio_response_demand.load(Ordering::Acquire);
        audio_capture.set_enabled(demanded);
        if (! demanded) {
            if (! audio_ended) {
                psw->endAudioResponse();
                audio_ended = true;
            }
            return;
        }
        wavsen::audio::AudioPcmWindow window {};
        if (audio_capture.snapshot(window)) {
            audio_ended = false;
            psw->setAudioPcmWindow(viewer::ConvertAudioWindow(window));
        }
#else
        if (! audio_response_demand.load(Ordering::Acquire)) {
            if (audio_capture.is_inited()) audio_capture.uninit();
            if (! audio_ended) {
                psw->endAudioResponse();
                audio_ended = true;
            }
            return;
        }
        if (! audio_capture.is_inited() && ! audio_capture.init()) return;
        audio_ended    = false;
        const auto now = Instant::now();
        if (now < next_audio_update) return;
        next_audio_update = now + Duration::from_millis(u64(33));
        wavsen::audio::AudioPcmWindow window {};
        if (audio_capture.snapshot(window)) {
            psw->setAudioPcmWindow(viewer::ConvertAudioWindow(window));
        }
#endif
    };

    // Bulk-scan path: WP_COMPILE_ONLY=N waits N seconds after scene load
    // to let the async shader-compile pass drain, then exits. Skips the
    // render loop so no swapchain present is required (which would deadlock
    // with a hidden window). Use together with WP_HEADLESS=1.
    int exit_code = 0;
    if (capture) {
        while (! capture_state->done.load(Ordering::Acquire)) {
            stdin_control.poll(*psw);
            apply_locked_mouse();
            rstd::thread::sleep(Duration::from_millis(u64(10)));
        }
        if (! capture_state->ok.load(Ordering::Acquire)) exit_code = 1;
    } else if (auto co = rstd::env::var_os("WP_COMPILE_ONLY"_str); co && ! co->is_empty()) {
        auto text    = co->as_os_str().to_str();
        auto seconds = text ? rstd::from_str<i32>(*text).unwrap_or(i32(2)) : i32(2);
        if (seconds <= i32()) seconds = i32(2);
#if __is_target_os(macos)
        const auto deadline = Instant::now() + Duration::from_secs(u64(seconds.to_primitive()));
        // Keep the Cocoa run loop alive while the render thread initializes
        // the GLFW surface on the main thread. A blocking sleep would leave
        // dispatch_sync_f waiting forever in headless compile-only runs.
        while (Instant::now() < deadline) {
            glfwWaitEventsTimeout(0.01);
        }
#else
        rstd::thread::sleep(Duration::from_secs(u64(seconds.to_primitive())));
#endif
    } else {
        while (! glfwWindowShouldClose(window)) {
#if __is_target_os(macos)
            // Keep the Cocoa run loop responsive while MoltenVK drives the
            // CAMetalLayer. A 30 Hz event wait can leave display-link/layer
            // updates pending across multiple presents and lower the
            // effective FIFO cadence. Rendering remains on the render thread;
            // this only services GLFW/Cocoa events more frequently.
            glfwWaitEventsTimeout(1.0 / 120.0);
#else
            glfwWaitEventsTimeout(1.0 / 30.0);
#endif
            stdin_control.poll(*psw);
            apply_locked_mouse();
            update_audio();
        }
    }
    delete psw;
    if (! args.texture_memory_output.is_empty()) {
        auto            stats = texture_memory.statistics();
        rstd::json::Map fields;
        auto            add = [&fields](ref<str> key, u64 value) {
            fields.insert(String::make(key), rstd::into<owe::Json>(value));
        };
        fields.insert("backend"_Str,
                      rstd::into<owe::Json>(String::make(stats.virtual_memory ? "virtual-memory"_str
                                                                              : "system"_str)));
        fields.insert("overflow"_Str, rstd::into<owe::Json>(stats.logical.overflow));
        add("live_bytes"_str, stats.logical.live_bytes);
        add("peak_live_bytes"_str, stats.logical.peak_live_bytes);
        add("allocations"_str, stats.logical.allocations);
        add("deallocations"_str, stats.logical.deallocations);
        add("failures"_str, stats.logical.failures);
        add("mapped_bytes"_str,
            stats.virtual_memory ? rstd::as_cast<u64>(stats.virtual_memory->mapped_bytes) : u64());
        add("mapped_bytes_peak"_str,
            stats.virtual_memory ? rstd::as_cast<u64>(stats.virtual_memory->mapped_bytes_peak)
                                 : u64());
        add("mappings"_str,
            stats.virtual_memory ? rstd::as_cast<u64>(stats.virtual_memory->mappings) : u64());
        add("mappings_peak"_str,
            stats.virtual_memory ? rstd::as_cast<u64>(stats.virtual_memory->mappings_peak) : u64());
        auto report = rstd::json::to_string(owe::Json::Object(rstd::move(fields)));
        auto path   = rstd::path::PathBuf::from(args.texture_memory_output.as_str());
        if (rstd::fs::write(path.as_path(), report.as_str().as_bytes()).is_err())
            eprintln("failed to write texture memory statistics");
    }
    // wgl.Clear();
    if (window != nullptr) glfwDestroyWindow(window);
    glfwTerminate();
    return exit_code;
}

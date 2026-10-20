import rstd;
import rstd.test;
import weweb;

using namespace rstd::prelude;
using namespace rstd::literals;
using rstd::io::eprintln;
using rstd::path::PathBuf;
using rstd::time::Duration;
using rstd::time::Instant;

namespace
{

int RunShutdownCase(weweb::BrowserHost& host, ref<str> mode) {
    auto resources = rstd::env::var("OWE_CEF_TEST_RESOURCES"_str).unwrap();
    auto temporary = rstd::fs::TempDir::make("owe-web-close"_str).unwrap();
    auto root      = PathBuf::from(temporary.path());
    auto html      = String::make("<body style='margin:0;background:red'><script>"_str);
    if (mode == "popup"_str || mode == "popup-first"_str) {
        html.push_str("const p=window.open('popup.html');"_str);
        if (mode == "popup-first"_str) {
            html.push_str("setTimeout(()=>p.close(),100);"_str);
        }
        html.push_str("setInterval(()=>{if(p"_str);
        if (mode == "popup-first"_str) html.push_str("&&p.closed"_str);
        html.push_str(")document.body.style.background='lime'},10);"_str);
    } else {
        html.push_str("document.body.style.background='lime';"_str);
    }
    html.push_str("</script>"_str);
    rstd::fs::write(root.join("index.html"_str).as_path(), html.as_str().as_bytes()).unwrap();
    rstd::fs::write(root.join("popup.html"_str).as_path(),
                    "<body style='background:blue'>popup</body>"_str.as_bytes())
        .unwrap();

    weweb::BrowserHost::InitOptions options;
    options.resources_dir          = PathBuf::from(resources.as_str());
    options.locales_dir            = options.resources_dir.join("locales"_str);
    options.cache_dir              = root.join("profile"_str);
    options.enable_audio           = false;
    options.shared_texture_enabled = false;
    if (! host.Init(options)) return 1;
    if (mode == "initialized"_str) {
        host.RequestClose();
        const bool passed = host.ShouldExit() && host.Shutdown() && host.Shutdown();
        eprintln("CEF shutdown case {}: passed={}", mode, passed);
        return passed ? 0 : 1;
    }

    int  paints = 0;
    bool ready  = false;
    bool failed = false;
    host.SetCpuPaintCallback([&](const weweb::CpuPaintFrame& frame) {
        ++paints;
        const auto* pixels = static_cast<const unsigned char*>(frame.buffer);
        ready              = ready || (pixels[0] == 0 && pixels[1] == 255 && pixels[2] == 0);
        if (mode == "reentrant"_str && ready) {
            if (host.Shutdown()) failed = true;
            host.RequestClose();
        }
    });
    weweb::WebManifest manifest;
    manifest.entry_html = "index.html"_Str;
    weweb::BrowserHost::OpenOptions open;
    open.shared_texture_enabled = false;
    open.frame_rate             = 30;
    if (! host.OpenWallpaper(manifest, root.as_path(), 64, 64, open)) {
        host.Shutdown();
        return 1;
    }
    if (host.OpenWallpaper(manifest, root.as_path(), 64, 64, open)) failed = true;
    if (host.Shutdown()) failed = true;
    if (mode == "creating"_str) {
        host.RequestClose();
        host.RequestClose();
        if (host.ShouldExit()) failed = true;
    }

    auto deadline        = Instant::now() + Duration::from_secs(u64(15));
    int  paints_at_close = -1;
    while (! host.ShouldExit()) {
        if (Instant::now() >= deadline) {
            eprintln("CEF shutdown case {} timed out: paints={} ready={} closing={}",
                     mode,
                     paints,
                     ready,
                     host.IsClosing());
            rstd::process::exit(i32(1));
        }
        if (host.IsClosing() && paints_at_close < 0) paints_at_close = paints;
        host.Pump();
        if (! host.IsClosing()) {
            host.Invalidate();
            if (ready) {
                if (mode == "threaded"_str) {
                    auto thread = rstd::thread::spawn([&host] {
                                      host.RequestClose();
                                  }).unwrap();
                    rstd::move(thread).join().unwrap();
                } else {
                    host.RequestClose();
                }
                host.RequestClose();
            }
        }
        rstd::thread::sleep(Duration::from_millis(u64(1)));
    }
    if (paints_at_close >= 0 && paints != paints_at_close) failed = true;
    if (mode == "creating"_str ? paints != 0 : ! ready) failed = true;
    if (! host.IsClosing() || ! host.Shutdown() || ! host.Shutdown()) failed = true;
    eprintln("CEF shutdown case {}: paints={} passed={}", mode, paints, ! failed);
    return failed ? 1 : 0;
}

} // namespace

int main(int argc, char** argv) {
    weweb::BrowserHost host;
    if (int result = host.RunOrExitIfHelper(argc, argv); result >= 0) return result;
    rstd::env::args_init(argc, argv);
    if (auto mode = rstd::env::var("OWE_CEF_SHUTDOWN_CASE"_str); mode.is_ok())
        return RunShutdownCase(host, mode->as_str());
    return rstd::test::run_registered().to_primitive();
}

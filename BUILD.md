# Building open-wallpaper-engine

OWE is built with [lito](https://github.com/litocpp/lito).  

## Requirements

| Dependency | Version | Notes |
|------------|---------|-------|
| Clang | 22+ | C++20 modules toolchain |
| lito | 0.3.0+ | C++ builder |
| CMake | 4.0+ | used for CMake dependencies |
| Ninja | recent | CMake dependency builds |
| pkg-config | - | system dependency discovery |
| Vulkan SDK | 1.1+ | loader and headers |
| GLFW3 | - | standalone viewers |
| liblz4 | - | `.pkg` decompression |
| EGL / GLESv2 / X11 / wayland-egl | - | web viewer presenters |

The complete release environment is declared in `environment.yml`.

## Build

The default workspace members are the scene and web viewers:

```bash
lito build --profile release
```

Build an individual package when only one surface is needed:

```bash
lito build -p owe-sceneviewer --profile release
lito build -p owe-waywallen-scene-renderer --profile release
lito build -p owe-waywallen-web-renderer --profile release
```

Release viewer binaries are written under:

```text
build/release/bin/owe-sceneviewer/SceneViewer
build/release/bin/owe-webviewer/WebViewer
```

## Plugin bundle

The release helper builds and installs `waywallen-bridge`, invokes lito for the OWE
plugin, and packages:

```bash
./scripts/build_waywallen_plugin.sh
```

The bundle contains:

```text
plugin.toml
files.txt
main.lua
wallpaper_engine/
bin/waywallen-wescene-renderer
bin/weweb/waywallen-weweb-renderer
bin/weweb/libcef.so
bin/weweb/locales/
```

## JavaScript engine

Scene scripts run on QuickJS-ng by default. The `release-sm` profile runs them on SpiderMonkey 140, a JIT, instead:
it is 10 to 25 times faster on script-heavy wallpapers such as More Physics and changes nothing for the others.

```bash
lito build -p owe-sceneviewer --profile release-sm
```

It needs the SpiderMonkey development package (`libmozjs-140-dev` on Debian and Ubuntu; pkg-config module
`mozjs-140`). Both engines are reached through the QuickJS C API: `src/Scene/Script/JsSpiderMonkey.cpp` implements the
part of `quickjs.h` that the script runtime uses on top of SpiderMonkey, so `Script.cpp` is the same for both.
`tests/engine-boundary/run.sh` builds the same conformance test against QuickJS and against that file and diffs the
results. SpiderMonkey contexts belong to the thread that made them, so each runtime owns an engine thread and calls from
other threads are forwarded to it; `OnEngine` in `Script.cpp` makes the public entry points run whole on that thread.
Tuning knobs for experiments: `OWE_SM_BASELINE_WARMUP`, `OWE_SM_ION_WARMUP`, `OWE_SM_OFFTHREAD`, `OWE_SM_NURSERY_KB`;
`OWE_JS_DEBUG=1` logs script exceptions and the number of cross-thread calls.

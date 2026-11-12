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

Scene scripts run on QuickJS-ng in the release bundle. Local builds run them on a JIT, which is 10 to 25 times faster on
script-heavy wallpapers such as More Physics and changes nothing for the others:

```bash
scripts/local-build.sh -p owe-sceneviewer      # V8 (profile release-v8), SpiderMonkey if V8 is not installed
OWE_LOCAL_ENGINE=spidermonkey scripts/local-build.sh -p owe-sceneviewer   # profile release-sm
lito build -p owe-sceneviewer --profile release   # QuickJS: what the release bundle uses
```

V8 needs node's headers and libraries (`libnode-dev` on Debian and Ubuntu: `/usr/include/node/v8.h`, `libv8.so`); the
script writes the pkg-config file the build uses (module `owe-v8`) to `~/.cache/owe/pkgconfig` the first time. It is
not a supported V8 embedding, so it is for local use only. SpiderMonkey needs `libmozjs-140-dev` (pkg-config module
`mozjs-140`). All three engines are reached through the QuickJS C API: `src/Scene/Script/JsV8.cpp` and
`JsSpiderMonkey.cpp` implement the part of `quickjs.h` that the script runtime uses on top of the engine, so `Script.cpp`
is the same for all. `tests/engine-boundary/run.sh` builds the same conformance test against each and diffs the results.
V8 and SpiderMonkey contexts belong to the thread that made them, so each runtime owns an engine thread and calls from
other threads are forwarded to it; `OnEngine` in `Script.cpp` makes the public entry points run whole on that thread.
On More Physics V8 spends 6.5 ms per frame in scripts against 9 ms on SpiderMonkey and 150 ms on QuickJS.
V8 knobs: `OWE_V8_FLAGS` (extra V8 flags), `OWE_V8_WORKERS`.
Tuning knobs for experiments: `OWE_SM_BASELINE_WARMUP`, `OWE_SM_ION_WARMUP`, `OWE_SM_OFFTHREAD`, `OWE_SM_NURSERY_KB`;
`OWE_JS_DEBUG=1` logs script exceptions and the number of cross-thread calls.

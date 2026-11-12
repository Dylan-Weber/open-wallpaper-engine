#!/bin/sh
# Builds js_api_conformance.cpp against QuickJS-ng, the SpiderMonkey facade and (when node's V8 headers are installed,
# V8_INCLUDE_DIR defaults to /usr/include/node) the V8 facade, and runs them.
#   run.sh [QUICKJS_SRC_DIR [QUICKJS_BUILD_DIR [MOZJS_INCLUDE_DIR]]]
# Without libmozjs-dev installed, point MOZJS_LIB at the runtime library (libmozjs-140.so.0).
set -eu
here=$(cd "$(dirname "$0")" && pwd)
qsrc=${1:-$HOME/jit-engines/qjs-ng}
qbuild=${2:-$HOME/jit-engines/build-qjs-o3}
mozinc=${3:-$(pkg-config --variable=includedir mozjs-140 2>/dev/null)/mozjs-140}
out=${OUT:-${TMPDIR:-/tmp}/owe-engine-boundary}
mkdir -p "$out"
CXX=${CXX:-g++}
$CXX -std=c++20 -O1 -I"$qsrc" "$here/js_api_conformance.cpp" -o "$out/conf-quickjs" "$qbuild/libqjs.a" -lm -lpthread
$CXX -std=c++20 -O1 -Wno-invalid-offsetof -I"$mozinc" -I"$qsrc" \
    "$here/js_api_conformance.cpp" "$here/../../src/Scene/Script/JsSpiderMonkey.cpp" -o "$out/conf-spidermonkey" \
    ${MOZJS_LIB:-$(pkg-config --libs mozjs-140 2>/dev/null || echo "-lmozjs-140")} -lpthread
v8inc=${V8_INCLUDE_DIR:-/usr/include/node}
have_v8=0
if [ -f "$v8inc/v8.h" ]; then
    $CXX -std=c++20 -O1 -Wno-invalid-offsetof -I"$v8inc" -I"$qsrc" \
        "$here/js_api_conformance.cpp" "$here/../../src/Scene/Script/JsV8.cpp" -o "$out/conf-v8" \
        -lv8 -lv8_libplatform -lv8_libbase -lpthread && have_v8=1
fi
"$out/conf-quickjs" > "$out/quickjs.txt" || true
"$out/conf-spidermonkey" > "$out/spidermonkey.txt" || true
tail -1 "$out/quickjs.txt" | sed 's/^/quickjs:       /'
tail -1 "$out/spidermonkey.txt" | sed 's/^/spidermonkey:  /'
diff "$out/quickjs.txt" "$out/spidermonkey.txt" && echo "backends agree"
if [ "$have_v8" = 1 ]; then
    "$out/conf-v8" > "$out/v8.txt" || true
    tail -1 "$out/v8.txt" | sed 's/^/v8:            /'
    diff "$out/quickjs.txt" "$out/v8.txt" && echo "v8 agrees"
fi

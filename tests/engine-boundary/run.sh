#!/bin/sh
# Builds js_api_conformance.cpp against QuickJS-ng and against the SpiderMonkey facade and runs both.
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
"$out/conf-quickjs" > "$out/quickjs.txt" || true
"$out/conf-spidermonkey" > "$out/spidermonkey.txt" || true
tail -1 "$out/quickjs.txt" | sed 's/^/quickjs:       /'
tail -1 "$out/spidermonkey.txt" | sed 's/^/spidermonkey:  /'
diff "$out/quickjs.txt" "$out/spidermonkey.txt" && echo "backends agree"

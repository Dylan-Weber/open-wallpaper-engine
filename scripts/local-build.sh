#!/usr/bin/env bash
# Local build of the viewers on a JIT script engine. The release bundle (scripts/build_waywallen_plugin.sh) keeps the
# QuickJS release profile.
#
#   scripts/local-build.sh [lito build arguments...]      e.g. scripts/local-build.sh -p owe-sceneviewer -j 4
#
# Engine, chosen by OWE_LOCAL_ENGINE (default: v8, falling back to spidermonkey when V8 is not installed):
#   v8            profile release-v8. Needs node's V8 headers and libraries (libnode-dev on Debian and Ubuntu:
#                 /usr/include/node/v8.h, libv8.so). The pkg-config file the build uses (module owe-v8) is written
#                 to ${XDG_CACHE_HOME:-~/.cache}/owe/pkgconfig on first use.
#   spidermonkey  profile release-sm. Needs libmozjs-140-dev (pkg-config module mozjs-140).
#   quickjs       profile release, as the release bundle.
# Without arguments it builds the default members (the scene and web viewers). Outputs are under build/<profile>/.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

engine=${OWE_LOCAL_ENGINE:-v8}

v8_available() {
    [ -f /usr/include/node/v8.h ] && [ -e /usr/lib/x86_64-linux-gnu/libv8.so ] && return 0
    pkg-config --exists owe-v8 2>/dev/null
}

if [ "$engine" = v8 ] && ! v8_available; then
    if [ -n "${OWE_LOCAL_ENGINE:-}" ]; then
        echo "local-build.sh: OWE_LOCAL_ENGINE=v8 but node's V8 headers/libraries were not found (install libnode-dev)." >&2
        exit 1
    fi
    echo "local-build.sh: V8 not installed (libnode-dev); falling back to SpiderMonkey. Set OWE_LOCAL_ENGINE to choose." >&2
    engine=spidermonkey
fi

case "$engine" in
v8)
    if ! pkg-config --exists owe-v8 2>/dev/null; then
        dir=${XDG_CACHE_HOME:-$HOME/.cache}/owe/pkgconfig
        mkdir -p "$dir"
        cat > "$dir/owe-v8.pc" <<'PC'
prefix=/usr
libdir=/usr/lib/x86_64-linux-gnu
includedir=/usr/include

Name: V8
Description: V8 from the distro's libnode-dev (not a supported embedding configuration; local use)
Version: 12.4

Libs: -L${libdir} -lv8 -lv8_libplatform -lv8_libbase
Cflags: -I${includedir}/node
PC
        export PKG_CONFIG_PATH="$dir${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
    fi
    profile=release-v8
    ;;
spidermonkey)
    if ! pkg-config --exists mozjs-140 2>/dev/null; then
        echo "local-build.sh: pkg-config module mozjs-140 not found (install libmozjs-140-dev, or set PKG_CONFIG_PATH);" >&2
        echo "                use 'lito build --profile release' for the QuickJS build." >&2
        exit 1
    fi
    profile=release-sm
    ;;
quickjs) profile=release ;;
*)
    echo "local-build.sh: unknown OWE_LOCAL_ENGINE '$engine' (v8, spidermonkey or quickjs)" >&2
    exit 1
    ;;
esac
echo "local-build.sh: script engine $engine (profile $profile)" >&2
exec lito build --profile "$profile" "$@"

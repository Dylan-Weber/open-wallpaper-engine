#!/usr/bin/env bash
# Local build of the viewers on SpiderMonkey, a JIT (profile release-sm; needs libmozjs-140-dev, pkg-config module
# mozjs-140). The release bundle (scripts/build_waywallen_plugin.sh) keeps the QuickJS release profile.
#
#   scripts/local-build.sh [lito build arguments...]      e.g. scripts/local-build.sh -p owe-sceneviewer -j 4
#
# Without arguments it builds the default members (the scene and web viewers). Outputs are under build/release-sm/.
# Use `lito build --profile release` for a QuickJS build.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
if ! pkg-config --exists mozjs-140 2>/dev/null; then
    echo "local-build.sh: pkg-config module mozjs-140 not found (install libmozjs-140-dev, or set PKG_CONFIG_PATH);" >&2
    echo "                use 'lito build --profile release' for the QuickJS build." >&2
    exit 1
fi
exec lito build --profile release-sm "$@"

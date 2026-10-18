#include <rstd/macro.hpp>

import rstd;
import waywallen.render_resolution;

int main() {
    using ww_wescene::RenderResolution;
    const auto web    = RenderResolution::describe(1920, 1080, false);
    const auto custom = web.resolve(WW_RESOLUTION_CUSTOM, 8192);
    rstd_assert(custom.width == 7680 && custom.height == 4320);
    const auto small = web.resolve(WW_RESOLUTION_CUSTOM, 540);
    rstd_assert(small.width == 960 && small.height == 540);
    const auto preset = web.resolve(WW_RESOLUTION_2160P, 100);
    rstd_assert(preset.width == 3840 && preset.height == 2160);
    const auto portrait =
        RenderResolution::describe(1080, 1920, false).resolve(WW_RESOLUTION_CUSTOM, 8192);
    rstd_assert(portrait.width == 4320 && portrait.height == 7680);
    const auto square =
        RenderResolution::describe(1000, 1000, false).resolve(WW_RESOLUTION_CUSTOM, 8192);
    rstd_assert(square.width == 4320 && square.height == 4320);
    const auto ultrawide =
        RenderResolution::describe(10000, 1000, false).resolve(WW_RESOLUTION_AUTO, 1080);
    rstd_assert(ultrawide.width == 7680 && ultrawide.height == 768);
    const auto scene  = RenderResolution::describe(10000, 5000, true);
    const auto origin = scene.resolve(WW_RESOLUTION_ORIGIN, 1080);
    rstd_assert(origin.width == 10000 && origin.height == 5000);
    rstd_assert(scene.capabilities.mode == 1 && web.capabilities.mode == 2);
}

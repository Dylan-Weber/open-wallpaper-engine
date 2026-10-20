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

    for (auto extent :
         rstd::array<ww_wescene::RenderExtent, 3> { ww_wescene::RenderExtent { 8192, 2576 },
                                                    ww_wescene::RenderExtent { 2048, 644 },
                                                    ww_wescene::RenderExtent { 4096, 1288 } }) {
        auto position = extent.normalizePointer(extent.width * 0.625f, extent.height * 0.25f);
        rstd_assert(position && (*position)[rstd::usize()] == 0.625 &&
                    (*position)[rstd::usize(1)] == 0.25);
        rstd_assert(extent.normalizePointer(0, 0).is_some());
        rstd_assert(extent.normalizePointer(float(extent.width), float(extent.height)).is_some());
        rstd_assert(extent.normalizePointer(-1, -1).is_none());
        rstd_assert(extent.normalizePointer(float(extent.width) + 1, 0).is_none());
        rstd_assert(extent.normalizePointer(0, float(extent.height) + 1).is_none());
    }
    rstd_assert((ww_wescene::RenderExtent { 0, 644 }).normalizePointer(0, 0).is_none());
    rstd_assert((ww_wescene::RenderExtent { 2048, 0 }).normalizePointer(0, 0).is_none());
}

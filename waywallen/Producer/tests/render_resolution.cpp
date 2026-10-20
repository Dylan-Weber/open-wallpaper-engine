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
    rstd_assert(scene.capabilities.mode == 2 && web.capabilities.mode == 2);
    const auto scene_custom = scene.resolve(WW_RESOLUTION_CUSTOM, 8192);
    rstd_assert(scene_custom.width == 16384 && scene_custom.height == 8192);
    const auto scene_auto = scene.resolve(WW_RESOLUTION_AUTO, 1080);
    rstd_assert(scene_auto.width == 7680 && scene_auto.height == 3840);
    const auto procedural = RenderResolution::describe(128, 128, true);
    rstd_assert(procedural.capabilities.mode == 2);
    rstd_assert(procedural.capabilities.reference_width == 128 &&
                procedural.capabilities.reference_height == 128);
    rstd_assert(procedural.capabilities.max_width >= 2560 &&
                procedural.capabilities.max_height >= 2560);
    waywallen_render_target_config_t config {
        .revision              = 1,
        .capabilities_revision = procedural.capabilities.revision,
        .width                 = 2560,
        .height                = 2560,
    };
    ww_pool_directive_t directive {};
    rstd_assert(ww_bridge_render_target_directive(&procedural.capabilities, &config, &directive) ==
                0);
    config.width = config.height = 8192;
    rstd_assert(ww_bridge_render_target_directive(&procedural.capabilities, &config, &directive) !=
                0);
    const auto procedural_origin = procedural.resolve(WW_RESOLUTION_ORIGIN, 1080);
    rstd_assert(procedural_origin.width == 128 && procedural_origin.height == 128);
    const auto procedural_custom = procedural.resolve(WW_RESOLUTION_CUSTOM, 1440);
    rstd_assert(procedural_custom.width == 1440 && procedural_custom.height == 1440);
    const auto procedural_limit = procedural.resolve(WW_RESOLUTION_CUSTOM, 8192);
    rstd_assert(procedural_limit.width == 8192 && procedural_limit.height == 8192);

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

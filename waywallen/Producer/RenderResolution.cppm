export module waywallen.render_resolution;

export import waywallen.bridge;
import rstd;

export namespace ww_wescene
{

struct RenderExtent {
    rstd::uint32_t width;
    rstd::uint32_t height;
};

struct RenderResolution {
    waywallen_render_target_capabilities_t capabilities;

    static RenderResolution describe(rstd::uint32_t width, rstd::uint32_t height, bool native) {
        return { { .revision         = 1,
                   .reference_width  = width,
                   .reference_height = height,
                   .mode             = native ? 1u : 2u,
                   .min_width        = 1,
                   .min_height       = 1,
                   .max_width        = native ? width : (width >= height ? 7680u : 4320u),
                   .max_height       = native ? height : (width >= height ? 4320u : 7680u),
                   .width_alignment  = 1,
                   .height_alignment = 1 } };
    }

    RenderExtent resolve(rstd::int32_t resolution, rstd::uint32_t custom_extent) const {
        RenderExtent extent { capabilities.reference_width, capabilities.reference_height };
        const auto cap = resolution == WW_RESOLUTION_CUSTOM ? custom_extent
                                                            : ww_resolution_short_edge(resolution);
        if (capabilities.mode != 2) {
            ww_resolution_apply_short_edge(
                cap, WW_RESOLUTION_CAP_ALLOW_UPSCALE, &extent.width, &extent.height);
            return extent;
        }
        // Clamp the scale before rounding either dimension.
        rstd::uint64_t numerator = cap ? cap : 1;
        rstd::uint64_t denominator =
            cap ? (extent.width < extent.height ? extent.width : extent.height) : 1;
        if (extent.width * numerator > capabilities.max_width * denominator) {
            numerator   = capabilities.max_width;
            denominator = extent.width;
        }
        if (extent.height * numerator > capabilities.max_height * denominator) {
            numerator   = capabilities.max_height;
            denominator = extent.height;
        }
        extent.width =
            static_cast<rstd::uint32_t>((extent.width * numerator + denominator - 1) / denominator);
        extent.height = static_cast<rstd::uint32_t>((extent.height * numerator + denominator - 1) /
                                                    denominator);
        return extent;
    }
};

} // namespace ww_wescene

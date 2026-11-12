export module waywallen.render_resolution;

export import waywallen.bridge;
import rstd;

export namespace ww_wescene
{

struct RenderExtent {
    rstd::uint32_t width;
    rstd::uint32_t height;

    auto normalizePointer(float x, float y) const -> rstd::Option<rstd::array<double, 2>> {
        // (-1, -1) is the pointer leaving; any other position is real, even past the extent
        // while a drag carries on beyond the picture.
        if (! width || ! height || (x - x) != 0.0f || (y - y) != 0.0f || (x == -1.0f && y == -1.0f))
            return rstd::None();
        return rstd::Some(rstd::array<double, 2> { double(x) / width, double(y) / height });
    }
};

struct RenderResolution {
    waywallen_render_target_capabilities_t capabilities;
    bool                                   has_origin;

    // The canvas widened to the aspect aspect_width : aspect_height, keeping its height: the extent a
    // scene is drawn at to show more of its world instead of being cropped by a wider output. A canvas
    // already at least that wide, or an unusable aspect, leaves it as it is.
    static RenderExtent extend(RenderExtent canvas, rstd::uint32_t aspect_width,
                               rstd::uint32_t aspect_height) {
        if (! canvas.width || ! canvas.height || ! aspect_width || ! aspect_height) return canvas;
        const rstd::uint64_t height = canvas.height;
        if (rstd::uint64_t(canvas.width) * aspect_height >= height * aspect_width) return canvas;
        const auto width = (height * aspect_width + aspect_height / 2) / aspect_height;
        if (width > 65535) return canvas;
        return { static_cast<rstd::uint32_t>(width), canvas.height };
    }

    static RenderResolution describe(rstd::uint32_t width, rstd::uint32_t height, bool has_origin) {
        // A scene canvas defines coordinates, not a ceiling on generated pixel detail.
        return { { .revision         = 1,
                   .reference_width  = width,
                   .reference_height = height,
                   .mode             = 2,
                   .min_width        = 1,
                   .min_height       = 1,
                   .max_width        = width >= height ? 7680u : 4320u,
                   .max_height       = width >= height ? 4320u : 7680u,
                   .width_alignment  = 1,
                   .height_alignment = 1 },
                 has_origin };
    }

    RenderExtent resolve(rstd::int32_t resolution, rstd::uint32_t custom_extent) const {
        RenderExtent extent { capabilities.reference_width, capabilities.reference_height };
        const auto cap = resolution == WW_RESOLUTION_CUSTOM ? custom_extent
                                                            : ww_resolution_short_edge(resolution);
        if (has_origin && resolution != WW_RESOLUTION_AUTO) {
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

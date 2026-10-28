module;

#include <rstd/macro.hpp>

module wescene.pkg.parse;
import :scene_context;
import rstd;
import rstd.log;
import wescene.load_bench;

using namespace rstd::prelude;
using namespace rstd::literals;
using rstd::sync::Arc;

auto owe::SceneParser::Parse(ref<str> scene_id, ref<wpscene::SceneDocument> document,
                             mut_ref<fs::VFS> vfs, mut_ref<wavsen::audio::SoundManager> sound,
                             SceneParseOptions options) -> Result<ParsedScene, SceneParseError> {
    auto&       vfs_owner   = *vfs.as_raw_ptr();
    auto&       sound_owner = *sound.as_raw_ptr();
    auto        total_span  = SceneLoadSpan(options.load_bench, &SceneLoadProbeIds::parse_total);
    const auto& metadata    = document->metadata;
    rstd_info("scene: pkg_version={} scene_json_version={}",
              static_cast<unsigned>(metadata.pkg_version),
              static_cast<unsigned>(metadata.scene_json_version));

    if (! document->objects_are_array) {
        return Err(SceneParseError {
            .kind    = SceneParseErrorKind::ObjectExpansion,
            .message = "scene objects must be an array"_Str,
        });
    }

    auto expanded = [&] {
        auto span = SceneLoadSpan(options.load_bench, &SceneLoadProbeIds::parse_expand_objects);
        return ExpandSceneObjects(document, vfs, options.user_properties);
    }();
    auto objects = rstd::move(expanded.objects);
    // The lights that cast a shadow map: as many of each kind as the light config allows (owe
    // draws one directional light's cascades).
    const auto& config = metadata.general.lightconfig;
    u32         directional_left { rstd::cmp::min(config.directionalshadow, u32(1)) };
    u32         spot_left { config.spotshadow };
    auto        shadow_lights = Vec<SceneShadowLight>::make();
    bool        has_shadow_caster { false };
    for (const auto& object : objects) {
        if (object.is_Light()) {
            const auto& light = object.as_Light().value;
            if (! light.castshadow) continue;
            if ((light.light == "directional"_str || light.light == "ldirectional"_str) &&
                directional_left > u32()) {
                --directional_left;
                shadow_lights.push(
                    SceneShadowLight { .id = light.id, .type = SceneLightType::Directional });
            } else if ((light.light == "spot"_str || light.light == "lspot"_str) &&
                       spot_left > u32()) {
                --spot_left;
                shadow_lights.push(SceneShadowLight { .id = light.id, .type = SceneLightType::Spot });
            }
        } else if (object.is_Model()) {
            has_shadow_caster = has_shadow_caster || object.as_Model().value.castshadow;
        }
    }
    if (! options.capabilities.directional_shadow || ! has_shadow_caster) shadow_lights.clear();

    auto context_span = SceneLoadSpan(options.load_bench, &SceneLoadProbeIds::parse_context);
    auto context =
        BuildContext(vfs_owner,
                     scene_id,
                     metadata,
                     ResolveOrthoProjectionExtent(metadata, objects.as_slice()),
                     options.user_properties,
                     rstd::move(options.shader_cache_dir),
                     GeometryShaderLimits {
                         .max_output_vertices = options.capabilities.max_geometry_output_vertices,
                         .max_total_output_components =
                             options.capabilities.max_geometry_total_output_components,
                     },
                     shadow_lights.as_slice(),
                     options.capabilities.geometry_shader_supported,
                     rstd::move(options.image_ready),
                     rstd::move(options.texture_memory));
    context.load_bench             = options.load_bench;
    auto runtime_input             = Arc<UniformRuntimeInput>::make(context.uniform_state.clone());
    context.hidden_link_source_ids = rstd::move(expanded.hidden_link_source_ids);
    context.linked_source_ids      = rstd::move(expanded.linked_source_ids);
    IndexSceneDocument(context, document, objects.as_slice());
    ProcessContainers(context, objects.as_mut_slice().as_mut_ref());
    (void)context_span.finish();

    {
        auto span = SceneLoadSpan(options.load_bench, &SceneLoadProbeIds::parse_objects);
        ProcessObjects(
            context, objects.as_mut_slice().as_mut_ref(), &sound_owner, {}, options.load_bench);
    }
    {
        auto span = SceneLoadSpan(options.load_bench, &SceneLoadProbeIds::parse_post_process);
        if (metadata.general.bloom) BuildBloomPostProcess(context, vfs_owner, metadata.general);
    }

    auto finalize_span = SceneLoadSpan(options.load_bench, &SceneLoadProbeIds::parse_finalize);
    return Ok(ParsedScene {
        .scene         = FinalizeScene(context),
        .runtime_input = rstd::move(runtime_input),
    });
}

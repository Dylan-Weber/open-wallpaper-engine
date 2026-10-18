module;
export module wescene.load_bench;

import rstd;
import rstd.bench;

using rstd::sync::Arc;

using namespace rstd::prelude;
using namespace rstd::literals;

export namespace owe
{

struct SceneLoadProbeIds {
    rstd::bench::probe::ProbeId viewer_startup;
    rstd::bench::probe::ProbeId preload_scene_document;
    rstd::bench::probe::ProbeId vulkan_init;
    rstd::bench::probe::ProbeId vulkan_instance;
    rstd::bench::probe::ProbeId vulkan_device;
    rstd::bench::probe::ProbeId vulkan_swapchain;
    rstd::bench::probe::ProbeId vulkan_resources;
    rstd::bench::probe::ProbeId load_total;
    rstd::bench::probe::ProbeId load_audio;
    rstd::bench::probe::ProbeId load_vfs_assets;
    rstd::bench::probe::ProbeId load_project_properties;
    rstd::bench::probe::ProbeId load_package;
    rstd::bench::probe::ProbeId load_scene_document;
    rstd::bench::probe::ProbeId parse_total;
    rstd::bench::probe::ProbeId parse_expand_objects;
    rstd::bench::probe::ProbeId parse_context;
    rstd::bench::probe::ProbeId parse_objects;
    rstd::bench::probe::ProbeId parse_object_image;
    rstd::bench::probe::ProbeId parse_object_particle;
    rstd::bench::probe::ProbeId parse_object_sound;
    rstd::bench::probe::ProbeId parse_object_light;
    rstd::bench::probe::ProbeId parse_object_text;
    rstd::bench::probe::ProbeId parse_object_model;
    rstd::bench::probe::ProbeId parse_object_camera;
    rstd::bench::probe::ProbeId parse_post_process;
    rstd::bench::probe::ProbeId parse_finalize;
    rstd::bench::probe::ProbeId load_runtime_setup;
    rstd::bench::probe::ProbeId load_initial_properties;
    rstd::bench::probe::ProbeId render_load;
    rstd::bench::probe::ProbeId render_snapshot;
    rstd::bench::probe::ProbeId render_graph_build;
    rstd::bench::probe::ProbeId render_graph_compile;
    rstd::bench::probe::ProbeId render_program_build;
    rstd::bench::probe::ProbeId render_requests;
    rstd::bench::probe::ProbeId render_resources_prepare;
    rstd::bench::probe::ProbeId render_graphics_prepare;
    rstd::bench::probe::ProbeId render_bindings_prepare;
    rstd::bench::probe::ProbeId render_renderer_compile;
    rstd::bench::probe::ProbeId render_texture_plan;
    rstd::bench::probe::ProbeId render_texture_decode;
    rstd::bench::probe::ProbeId render_texture_upload_prepare;
    rstd::bench::probe::ProbeId render_texture_upload_wait;
    rstd::bench::probe::ProbeId render_scopes;
    rstd::bench::probe::ProbeId render_upload_submit;
    rstd::bench::probe::ProbeId render_user_property;
    rstd::bench::probe::ProbeId render_user_property_apply;
    rstd::bench::probe::ProbeId render_user_property_refresh;
    rstd::bench::probe::ProbeId render_user_property_graph_rebuild;
    rstd::bench::probe::ProbeId render_first_frame;
    rstd::bench::probe::ProbeId render_first_frame_prepare;
    rstd::bench::probe::ProbeId render_first_draw;
    rstd::bench::probe::ProbeId material_build;
    rstd::bench::probe::ProbeId material_sources;
    rstd::bench::probe::ProbeId material_texture_metadata;
    rstd::bench::probe::ProbeId material_preprocess;
    rstd::bench::probe::ProbeId material_combos;
    rstd::bench::probe::ProbeId material_variant;
    rstd::bench::probe::ProbeId material_compile;
    rstd::bench::probe::ProbeId model_read;
    rstd::bench::probe::ProbeId model_mesh;
    rstd::bench::probe::ProbeId particle_finalize;
    rstd::bench::probe::ProbeId shader_source_cache;
    rstd::bench::probe::ProbeId shader_includes;
    rstd::bench::probe::ProbeId shader_annotations;
    rstd::bench::probe::ProbeId shader_source_store;
    rstd::bench::probe::ProbeId shader_cache_key;
    rstd::bench::probe::ProbeId shader_cache_memory;
    rstd::bench::probe::ProbeId shader_cache_read;
    rstd::bench::probe::ProbeId shader_cache_decode;
    rstd::bench::probe::ProbeId shader_cache_restore;
    rstd::bench::probe::ProbeId shader_cache_store;
    rstd::bench::probe::ProbeId shader_preprocess;
    rstd::bench::probe::ProbeId shader_lower;
    rstd::bench::probe::ProbeId shader_backend;
    rstd::bench::probe::ProbeId shader_reflect;
    rstd::bench::probe::ProbeId shader_reflect_stage_hash;
    rstd::bench::probe::ProbeId shader_reflect_spirv;
    rstd::bench::probe::ProbeId shader_interface_apply;
    rstd::bench::probe::ProbeId render_frame_animations;
    rstd::bench::probe::ProbeId render_frame_scripts;
    rstd::bench::probe::ProbeId render_frame_runtime;
    rstd::bench::probe::ProbeId render_frame_video;
    rstd::bench::probe::ProbeId render_frame_fonts;
    rstd::bench::probe::ProbeId render_changes_flush;
    rstd::bench::probe::ProbeId image_puppet;
    rstd::bench::probe::ProbeId image_puppet_layer;
    rstd::bench::probe::ProbeId image_geometry;
    rstd::bench::probe::ProbeId image_effects;
    rstd::bench::probe::ProbeId image_finalize;
    rstd::bench::probe::ProbeId animation_bindings;
};

struct SceneLoadBenchRecorderView {
    rstd::bench::probe::ProbeRecorder* recorder { nullptr };
    const SceneLoadProbeIds*           ids { nullptr };

    explicit operator bool() const noexcept { return recorder != nullptr && ids != nullptr; }
};

using SceneLoadSpanGuard = rstd::bench::probe::SpanGuard;

auto SceneLoadSpan(SceneLoadBenchRecorderView  view,
                   rstd::bench::probe::ProbeId SceneLoadProbeIds::* probe) noexcept
    -> rstd::bench::probe::SpanGuard {
    return view ? view.recorder->span(view.ids->*probe) : rstd::bench::probe::SpanGuard {};
}

template<typename Writer>
    requires Impled<Writer, rstd::io::Write>
auto WriteSceneLoadTimeline(Writer& writer, slice<rstd::bench::probe::ProbeBatch> batches)
    -> rstd::io::Result<empty> {
    // Recorders share a clock origin, but parent sequences are recorder-local; zero is a root.
    for (const auto& batch : batches) {
        for (const auto& sample : batch.samples()) {
            auto line    = rstd::format("sample={} recorder={} thread={} sequence={} parent={} "
                                        "started_ns={} elapsed_ns={}\n",
                                        batch.schema()->label(sample.probe).unwrap(),
                                        sample.recorder_id,
                                        sample.thread_id.as_u64().get(),
                                        sample.sequence,
                                        sample.parent_sequence ? *sample.parent_sequence : u64(),
                                        sample.started_ns,
                                        sample.elapsed.as_nanos());
            auto written = rstd::io::write_all(writer, line.as_str().as_bytes());
            if (written.is_err()) return written;
        }
    }
    return Ok(empty {});
}

class SceneLoadBenchContext;
using SceneLoadBenchHandle = Arc<SceneLoadBenchContext>;

class SceneLoadBenchContext {
public:
    SceneLoadBenchContext(const SceneLoadBenchContext&)                    = delete;
    auto operator=(const SceneLoadBenchContext&) -> SceneLoadBenchContext& = delete;

    auto run_id() const noexcept -> u64 { return m_run_id; }
    auto ids() const noexcept -> const SceneLoadProbeIds& { return m_ids; }
    auto session() const noexcept -> const rstd::bench::probe::ProbeSession& { return m_session; }
    auto schema_owner() const noexcept -> Arc<rstd::bench::probe::ProbeSchema> {
        return m_schema.clone();
    }
    auto output_path() const noexcept -> ref<rstd::path::Path> { return m_output_path.as_path(); }

    void add_preload_batch(rstd::bench::probe::ProbeBatch batch) {
        m_preload_batches.push(rstd::move(batch));
    }

    auto take_preload_batches() -> Vec<rstd::bench::probe::ProbeBatch> {
        auto batches      = rstd::move(m_preload_batches);
        m_preload_batches = Vec<rstd::bench::probe::ProbeBatch>::make();
        return batches;
    }

private:
    struct FactoryToken {};
    friend auto CreateSceneLoadBench(ref<str>) -> Option<SceneLoadBenchHandle>;

public:
    SceneLoadBenchContext(FactoryToken, rstd::path::PathBuf output_path, SceneLoadProbeIds ids,
                          Arc<rstd::bench::probe::ProbeSchema> schema, u64 run_id)
        : m_output_path(rstd::move(output_path)),
          m_ids(ids),
          m_schema(schema.clone()),
          m_session(rstd::bench::probe::ProbeSession::new_(rstd::move(schema))),
          m_run_id(run_id),
          m_preload_batches(Vec<rstd::bench::probe::ProbeBatch>::make()) {}

    rstd::path::PathBuf                  m_output_path;
    SceneLoadProbeIds                    m_ids;
    Arc<rstd::bench::probe::ProbeSchema> m_schema;
    rstd::bench::probe::ProbeSession     m_session;
    u64                                  m_run_id;
    Vec<rstd::bench::probe::ProbeBatch>  m_preload_batches;
};

auto CreateSceneLoadBench(ref<str> output_path) -> Option<SceneLoadBenchHandle>;

} // namespace owe

namespace
{

auto RegisterProbe(rstd::bench::probe::ProbeRegistry& registry, ref<str> label)
    -> rstd::bench::probe::ProbeId {
    auto registered = registry.register_probe(label);
    if (registered.is_err()) rstd::panic { "scene load probe catalog exhausted" };
    return rstd::move(registered).unwrap_unchecked();
}

} // namespace

namespace owe
{

auto CreateSceneLoadBench(ref<str> output_path) -> Option<SceneLoadBenchHandle> {
    if (output_path.is_empty()) return None();

    auto              registry = rstd::bench::probe::ProbeRegistry::new_();
    SceneLoadProbeIds ids {
        .viewer_startup           = RegisterProbe(registry, "viewer.startup"_str),
        .preload_scene_document   = RegisterProbe(registry, "preload.scene_document"_str),
        .vulkan_init              = RegisterProbe(registry, "vulkan.init"_str),
        .vulkan_instance          = RegisterProbe(registry, "vulkan.instance"_str),
        .vulkan_device            = RegisterProbe(registry, "vulkan.device"_str),
        .vulkan_swapchain         = RegisterProbe(registry, "vulkan.swapchain"_str),
        .vulkan_resources         = RegisterProbe(registry, "vulkan.resources"_str),
        .load_total               = RegisterProbe(registry, "load.total"_str),
        .load_audio               = RegisterProbe(registry, "load.audio"_str),
        .load_vfs_assets          = RegisterProbe(registry, "load.vfs.assets"_str),
        .load_project_properties  = RegisterProbe(registry, "load.project_properties"_str),
        .load_package             = RegisterProbe(registry, "load.package"_str),
        .load_scene_document      = RegisterProbe(registry, "load.scene_document"_str),
        .parse_total              = RegisterProbe(registry, "parse.total"_str),
        .parse_expand_objects     = RegisterProbe(registry, "parse.expand_objects"_str),
        .parse_context            = RegisterProbe(registry, "parse.context"_str),
        .parse_objects            = RegisterProbe(registry, "parse.objects"_str),
        .parse_object_image       = RegisterProbe(registry, "parse.object.image"_str),
        .parse_object_particle    = RegisterProbe(registry, "parse.object.particle"_str),
        .parse_object_sound       = RegisterProbe(registry, "parse.object.sound"_str),
        .parse_object_light       = RegisterProbe(registry, "parse.object.light"_str),
        .parse_object_text        = RegisterProbe(registry, "parse.object.text"_str),
        .parse_object_model       = RegisterProbe(registry, "parse.object.model"_str),
        .parse_object_camera      = RegisterProbe(registry, "parse.object.camera"_str),
        .parse_post_process       = RegisterProbe(registry, "parse.post_process"_str),
        .parse_finalize           = RegisterProbe(registry, "parse.finalize"_str),
        .load_runtime_setup       = RegisterProbe(registry, "load.runtime_setup"_str),
        .load_initial_properties  = RegisterProbe(registry, "load.initial_properties"_str),
        .render_load              = RegisterProbe(registry, "render.load"_str),
        .render_snapshot          = RegisterProbe(registry, "render.snapshot"_str),
        .render_graph_build       = RegisterProbe(registry, "render.graph.build"_str),
        .render_graph_compile     = RegisterProbe(registry, "render.graph.compile"_str),
        .render_program_build     = RegisterProbe(registry, "render.program.build"_str),
        .render_requests          = RegisterProbe(registry, "render.requests"_str),
        .render_resources_prepare = RegisterProbe(registry, "render.resources.prepare"_str),
        .render_graphics_prepare  = RegisterProbe(registry, "render.graphics.prepare"_str),
        .render_bindings_prepare  = RegisterProbe(registry, "render.bindings.prepare"_str),
        .render_renderer_compile  = RegisterProbe(registry, "render.renderer.compile"_str),
        .render_texture_plan      = RegisterProbe(registry, "render.resources.texture.plan"_str),
        .render_texture_decode    = RegisterProbe(registry, "render.resources.texture.decode"_str),
        .render_texture_upload_prepare =
            RegisterProbe(registry, "render.resources.texture.upload.prepare"_str),
        .render_texture_upload_wait =
            RegisterProbe(registry, "render.resources.texture.upload.wait"_str),
        .render_scopes                = RegisterProbe(registry, "render.scopes"_str),
        .render_upload_submit         = RegisterProbe(registry, "render.upload.submit"_str),
        .render_user_property         = RegisterProbe(registry, "render.user_property"_str),
        .render_user_property_apply   = RegisterProbe(registry, "render.user_property.apply"_str),
        .render_user_property_refresh = RegisterProbe(registry, "render.user_property.refresh"_str),
        .render_user_property_graph_rebuild =
            RegisterProbe(registry, "render.user_property.graph_rebuild"_str),
        .render_first_frame         = RegisterProbe(registry, "render.first_frame"_str),
        .render_first_frame_prepare = RegisterProbe(registry, "render.first_frame.prepare"_str),
        .render_first_draw          = RegisterProbe(registry, "render.first_draw"_str),
        .material_build             = RegisterProbe(registry, "parse.material"_str),
        .material_sources           = RegisterProbe(registry, "parse.material.sources"_str),
        .material_texture_metadata = RegisterProbe(registry, "parse.material.texture_metadata"_str),
        .material_preprocess       = RegisterProbe(registry, "parse.material.preprocess"_str),
        .material_combos           = RegisterProbe(registry, "parse.material.combos"_str),
        .material_variant          = RegisterProbe(registry, "parse.material.variant"_str),
        .material_compile          = RegisterProbe(registry, "parse.material.compile"_str),
        .model_read                = RegisterProbe(registry, "parse.model.read"_str),
        .model_mesh                = RegisterProbe(registry, "parse.model.mesh"_str),
        .particle_finalize         = RegisterProbe(registry, "parse.particle.finalize"_str),
        .shader_source_cache       = RegisterProbe(registry, "parse.shader.source_cache"_str),
        .shader_includes           = RegisterProbe(registry, "parse.shader.includes"_str),
        .shader_annotations        = RegisterProbe(registry, "parse.shader.annotations"_str),
        .shader_source_store       = RegisterProbe(registry, "parse.shader.source_store"_str),
        .shader_cache_key          = RegisterProbe(registry, "parse.shader.cache.key"_str),
        .shader_cache_memory       = RegisterProbe(registry, "parse.shader.cache.memory_hit"_str),
        .shader_cache_read         = RegisterProbe(registry, "parse.shader.cache.read"_str),
        .shader_cache_decode       = RegisterProbe(registry, "parse.shader.cache.decode"_str),
        .shader_cache_restore      = RegisterProbe(registry, "parse.shader.cache.restore"_str),
        .shader_cache_store        = RegisterProbe(registry, "parse.shader.cache.store"_str),
        .shader_preprocess         = RegisterProbe(registry, "parse.shader.preprocess"_str),
        .shader_lower              = RegisterProbe(registry, "parse.shader.lower"_str),
        .shader_backend            = RegisterProbe(registry, "parse.shader.backend"_str),
        .shader_reflect            = RegisterProbe(registry, "parse.shader.reflect"_str),
        .shader_reflect_stage_hash = RegisterProbe(registry, "parse.shader.reflect.stage_hash"_str),
        .shader_reflect_spirv      = RegisterProbe(registry, "parse.shader.reflect.spirv"_str),
        .shader_interface_apply    = RegisterProbe(registry, "parse.shader.interface.apply"_str),
        .render_frame_animations   = RegisterProbe(registry, "render.frame.animations"_str),
        .render_frame_scripts      = RegisterProbe(registry, "render.frame.scripts"_str),
        .render_frame_runtime      = RegisterProbe(registry, "render.frame.runtime"_str),
        .render_frame_video        = RegisterProbe(registry, "render.frame.video"_str),
        .render_frame_fonts        = RegisterProbe(registry, "render.frame.fonts"_str),
        .render_changes_flush      = RegisterProbe(registry, "render.changes.flush"_str),
        .image_puppet              = RegisterProbe(registry, "parse.image.puppet"_str),
        .image_puppet_layer        = RegisterProbe(registry, "parse.image.puppet.layer"_str),
        .image_geometry            = RegisterProbe(registry, "parse.image.geometry"_str),
        .image_effects             = RegisterProbe(registry, "parse.image.effects"_str),
        .image_finalize            = RegisterProbe(registry, "parse.image.finalize"_str),
        .animation_bindings        = RegisterProbe(registry, "parse.animation.bindings"_str),
    };
    auto schema = rstd::move(registry).freeze();
    auto path   = rstd::path::PathBuf::from(output_path);

    static rstd::sync::atomic::Atomic<u64> next_run_id { u64(1) };
    auto run_id = next_run_id.fetch_add(u64(1), rstd::sync::atomic::Ordering::Relaxed);
    return Some(SceneLoadBenchHandle::make(
        SceneLoadBenchContext::FactoryToken {}, rstd::move(path), ids, rstd::move(schema), run_id));
}

} // namespace owe

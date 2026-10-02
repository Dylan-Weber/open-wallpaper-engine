module wescene.scene_render_pipeline;
import rstd.bench;

using namespace rstd::prelude;
using namespace rstd::literals;

namespace owe
{
namespace
{
template<class Id>
bool SameId(Id lhs, Id rhs) {
    return lhs.index == rhs.index && lhs.generation == rhs.generation;
}

vulkan::PassInvalidationFlags MaterialInvalidation(SceneMaterialDirtyFlags flags) {
    vulkan::PassInvalidationFlags out = vulkan::PassInvalidationNone;
    if ((flags & SceneMaterialDirtyResources) != 0)
        out |= vulkan::ToPassInvalidationFlags(vulkan::PassInvalidation::Resources);
    if ((flags & SceneMaterialDirtyPipeline) != 0)
        out |= vulkan::ToPassInvalidationFlags(vulkan::PassInvalidation::Pipeline) |
               vulkan::ToPassInvalidationFlags(vulkan::PassInvalidation::Framebuffer);
    return out;
}
} // namespace

SceneRenderPipeline::SceneRenderPipeline(Scene& scene, UniformRuntimeInput& input,
                                         vulkan::VulkanRender&      renderer,
                                         SceneRenderPipelineOptions options)
    : m_scene(scene), m_input(input), m_render(renderer), m_options(options) {}

bool SceneRenderPipeline::hasGraph() const { return m_graph.is_some(); }

auto SceneRenderPipeline::pending() const -> PendingScenePreparation {
    return { m_graph_dirty,
             m_textures_dirty,
             m_frame_work != FrameWork::Idle,
             m_meshes.len(),
             m_materials.len() };
}

auto SceneRenderPipeline::failed(vulkan::RenderPrepareError error)
    -> Result<empty, vulkan::RenderPrepareError> {
    m_ready = false;
    ++m_stats.preparation_failures;
    return Err(rstd::move(error));
}

void SceneRenderPipeline::setFillMode(FillMode mode) {
    m_options.fill_mode = mode;
    if (m_render.inited()) m_render.UpdateCameraFillMode(m_scene, mode);
}

bool SceneRenderPipeline::resize(unsigned width, unsigned height) {
    if (! m_render.onSwapchainReady(width, height)) return false;
    if (hasGraph()) return rebuild().is_ok();
    return true;
}

void SceneRenderPipeline::captureChanges() {
    m_graph_dirty |= m_scene.ConsumeRenderGraphDirty();
    auto targets = m_scene.ConsumePreparedRenderTargetDirtyEvents();
    m_textures_dirty |= ! targets.is_empty();
    for (auto event : m_scene.ConsumePreparedMeshDirtyEvents()) {
        bool merged {};
        for (auto& pending : m_meshes) {
            if (! SameId(pending.mesh, event.mesh)) continue;
            pending.flags |= event.flags;
            merged = true;
            break;
        }
        if (! merged) m_meshes.push(SceneMeshDirtyEvent(event));
        m_graph_dirty |= (event.flags & SceneMeshDirtyLayout) != 0;
    }
    for (auto event : m_scene.ConsumePreparedMaterialDirtyEvents()) {
        bool merged {};
        for (auto& pending : m_materials) {
            if (! SameId(pending.material, event.material)) continue;
            pending.flags |= event.flags;
            merged = true;
            break;
        }
        if (! merged) m_materials.push(SceneMaterialDirtyEvent(event));
        m_graph_dirty |= (event.flags & SceneMaterialDirtyGraph) != 0;
    }
}

auto SceneRenderPipeline::flushChanges(SceneLoadBenchRecorderView bench)
    -> Result<empty, vulkan::RenderPrepareError> {
    if (! m_render.inited() || (! hasGraph() && ! m_graph_dirty))
        return Err(vulkan::RenderPrepareError { vulkan::RenderPrepareStage::State });
    if (m_graph_dirty) {
        [[maybe_unused]] auto reconciliation = m_scene.AudioDemandMut()->BeginReconciliation();
        auto                  cleared        = m_render.tryClearLastRenderGraph(m_retention);
        if (cleared.is_err()) return cleared;
        if (m_evict_meshes) m_render.evictUnusedMeshes();
        m_render.configureRenderTargets(m_scene);
        {
            auto span  = SceneLoadSpan(bench, &SceneLoadProbeIds::render_snapshot);
            m_snapshot = ExtractRenderSceneSnapshot(m_scene);
        }
        {
            auto span = SceneLoadSpan(bench, &SceneLoadProbeIds::render_graph_build);
            m_graph   = Some(sceneToRenderGraph(m_scene, m_snapshot));
        }
        if (m_options.graphviz) (*m_graph)->ToGraphviz("graph.dot"_str);
        auto compiled = m_render.tryCompileRenderGraph(m_scene, **m_graph, m_snapshot, bench);
        if (compiled.is_err()) return compiled;
        m_render.UpdateCameraFillMode(m_scene, m_options.fill_mode);
        (void)m_scene.ConsumePreparedMaterialDirtyEvents();
        (void)m_scene.ConsumePreparedMeshDirtyEvents();
        (void)m_scene.ConsumePreparedRenderTargetDirtyEvents();
        (void)m_scene.ConsumeRenderGraphDirty();
        ++m_stats.graph_builds;
        m_graph_dirty  = false;
        m_evict_meshes = false;
        m_retention    = vulkan::RenderGraphResourceRetention::KeepSceneTextures;
    } else if (m_textures_dirty || ! m_meshes.is_empty() || ! m_materials.is_empty()) {
        Vec<vulkan::RenderItemPreparation> items;
        Vec<SceneMaterialId>               textures;
        auto invalidate = [&](slice<RenderItemId> ids, vulkan::PassInvalidationFlags flags) {
            for (auto id : ids) {
                bool merged {};
                for (auto& item : items) {
                    if (! SameId(item.item, id)) continue;
                    item.flags |= flags;
                    merged = true;
                    break;
                }
                if (! merged) items.push({ id, flags });
            }
        };
        for (const auto& event : m_meshes)
            invalidate(m_snapshot.renderItemsFor(event.mesh),
                       vulkan::ToPassInvalidationFlags(vulkan::PassInvalidation::Resources));
        for (const auto& event : m_materials) {
            if ((event.flags & SceneMaterialDirtyTextureBindings) != 0)
                textures.push(SceneMaterialId(event.material));
            auto flags = MaterialInvalidation(event.flags);
            if (flags != vulkan::PassInvalidationNone)
                invalidate(m_snapshot.renderItemsFor(event.material), flags);
        }
        auto prepared =
            m_render.tryPrepareResources(m_scene,
                                         m_snapshot,
                                         { items.as_slice(),
                                           textures.as_slice(),
                                           m_meshes.is_empty() && m_materials.is_empty() });
        if (prepared.is_err()) {
            if (prepared.unwrap_err().stage != vulkan::RenderPrepareStage::RebuildRequired)
                return prepared;
            m_graph_dirty = true;
            return flushChanges(bench);
        }
        if (m_textures_dirty) ++m_stats.texture_refreshes;
        m_stats.mesh_refreshes += m_meshes.len().to_primitive();
        for (const auto& event : m_materials)
            if (MaterialInvalidation(event.flags) != vulkan::PassInvalidationNone)
                ++m_stats.material_refreshes;
    }
    m_meshes.clear();
    m_materials.clear();
    m_textures_dirty = false;
    return Ok(empty {});
}

auto SceneRenderPipeline::rebuild(vulkan::RenderGraphResourceRetention retention, bool evict_meshes,
                                  SceneLoadBenchRecorderView bench)
    -> Result<empty, vulkan::RenderPrepareError> {
    m_ready       = false;
    m_graph_dirty = true;
    m_evict_meshes |= evict_meshes;
    if (retention == vulkan::RenderGraphResourceRetention::ReleaseSceneTextures)
        m_retention = retention;
    captureChanges();
    auto result = flushChanges(bench);
    if (result.is_err()) return failed(result.unwrap_err());
    m_ready = m_frame_work == FrameWork::Idle;
    return result;
}

auto SceneRenderPipeline::prepare(const script::FrameInputs& inputs,
                                  SceneLoadBenchRecorderView bench)
    -> Result<empty, vulkan::RenderPrepareError> {
    if (! m_render.inited() || ! hasGraph() || m_frame_work != FrameWork::Idle)
        return failed(vulkan::RenderPrepareError { vulkan::RenderPrepareStage::State });
    m_ready      = false;
    m_frame_time = inputs.frametime;
    m_scene.SetPointerPosition({ inputs.cursor_x, inputs.cursor_y });
    m_scene.SetPointerInWindow(inputs.cursor_in_window);
    m_input.SetPointerInput(inputs.cursor_x, inputs.cursor_y);
    m_input.SetAudioSpectrum(inputs.audio);
    m_scene.TickNodeFieldAnimations();
    script::TickSceneScripts(m_scene, inputs);
    m_scene.TickCameraPaths();
    m_scene.TickMaterialShaderAnimations();
    m_scene.TickTransformUpdaters();
    m_frame_work = FrameWork::BeforeRuntime;
    return retryPreparation(bench);
}

auto SceneRenderPipeline::retryPreparation(SceneLoadBenchRecorderView bench)
    -> Result<empty, vulkan::RenderPrepareError> {
    m_ready = false;
    if (! m_render.inited() || (! hasGraph() && ! m_graph_dirty))
        return failed(vulkan::RenderPrepareError { vulkan::RenderPrepareStage::State });
    captureChanges();
    if (m_frame_work == FrameWork::BeforeRuntime) {
        if (m_graph_dirty) {
            auto rebuilt = flushChanges(bench);
            if (rebuilt.is_err()) return failed(rebuilt.unwrap_err());
        }
        m_scene.Runtime().BeforeRender();
        m_frame_work = FrameWork::Resources;
        captureChanges();
    }
    // New mutations during a failed frame are merged without repeating scripts or runtime work.
    auto flushed = flushChanges(bench);
    if (flushed.is_err()) return failed(flushed.unwrap_err());
    if (m_frame_work == FrameWork::Resources) m_frame_work = FrameWork::Video;
    if (m_frame_work == FrameWork::Video) {
        auto video = m_render.tryPumpVideoTextures(m_frame_time);
        if (video.is_err()) return failed(video.unwrap_err());
        m_frame_work = FrameWork::Fonts;
    }
    auto fonts = m_render.tryPumpFontAtlases(m_scene);
    if (fonts.is_err()) return failed(fonts.unwrap_err());
    if (m_frame_work != FrameWork::Idle) ++m_stats.prepared_frames;
    m_frame_work = FrameWork::Idle;
    m_ready      = true;
    return Ok(empty {});
}

auto SceneRenderPipeline::refreshMaterials() -> Result<empty, vulkan::RenderPrepareError> {
    m_ready = false;
    captureChanges();
    auto result = flushChanges({});
    if (result.is_err()) return failed(result.unwrap_err());
    m_ready = m_frame_work == FrameWork::Idle;
    return result;
}

} // namespace owe

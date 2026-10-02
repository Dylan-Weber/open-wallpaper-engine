export module wescene.scene_render_pipeline;
import rstd;
import wescene.core;
export import wescene.scene;
export import wescene.vulkan_render;
export import wescene.script;
export import wescene.pkg.parse;
import wescene.rgraph;
import wescene.types;
import wescene.load_bench;

using namespace rstd::prelude;

export namespace owe
{

struct SceneRenderPipelineOptions {
    FillMode fill_mode { FillMode::ASPECTCROP };
    bool     graphviz { false };
};

struct SceneRenderPipelineStats {
    rstd::uint64_t graph_builds {};
    rstd::uint64_t prepared_frames {};
    rstd::uint64_t mesh_refreshes {};
    rstd::uint64_t material_refreshes {};
    rstd::uint64_t texture_refreshes {};
    rstd::uint64_t preparation_failures {};
};

struct PendingScenePreparation {
    bool  graph {}, textures {}, frame {};
    usize meshes {}, materials {};
};

// The borrowed scene, input and renderer must outlive this pipeline; calls are serialized.
class SceneRenderPipeline : NoCopy, NoMove {
public:
    SceneRenderPipeline(Scene&, UniformRuntimeInput&, vulkan::VulkanRender&,
                        SceneRenderPipelineOptions = {});
    auto rebuild(vulkan::RenderGraphResourceRetention =
                     vulkan::RenderGraphResourceRetention::KeepSceneTextures,
                 bool evict_meshes = false, SceneLoadBenchRecorderView = {})
        -> Result<empty, vulkan::RenderPrepareError>;
    // Updates scene state and GPU preparation; the host owns drawing and time advancement.
    auto prepare(const script::FrameInputs&, SceneLoadBenchRecorderView = {})
        -> Result<empty, vulkan::RenderPrepareError>;
    auto retryPreparation(SceneLoadBenchRecorderView = {})
        -> Result<empty, vulkan::RenderPrepareError>;
    auto refreshMaterials() -> Result<empty, vulkan::RenderPrepareError>;
    void setFillMode(FillMode);
    bool resize(unsigned width, unsigned height);
    bool hasGraph() const;
    bool ready() const { return m_ready && hasGraph() && m_render.inited(); }
    auto pending() const -> PendingScenePreparation;
    auto snapshot() const -> const RenderSceneSnapshot& { return m_snapshot; }
    auto stats() const -> SceneRenderPipelineStats { return m_stats; }

private:
    void captureChanges();
    auto flushChanges(SceneLoadBenchRecorderView) -> Result<empty, vulkan::RenderPrepareError>;
    auto failed(vulkan::RenderPrepareError) -> Result<empty, vulkan::RenderPrepareError>;
    enum class FrameWork
    {
        Idle,
        BeforeRuntime,
        Resources,
        Video,
        Fonts
    };
    FrameWork m_frame_work { FrameWork::Idle };
    double    m_frame_time {};
    bool      m_ready {}, m_graph_dirty {}, m_textures_dirty {}, m_evict_meshes {};
    vulkan::RenderGraphResourceRetention m_retention {
        vulkan::RenderGraphResourceRetention::KeepSceneTextures
    };
    Vec<SceneMeshDirtyEvent>     m_meshes;
    Vec<SceneMaterialDirtyEvent> m_materials;
    Scene&                       m_scene;
    UniformRuntimeInput&         m_input;
    vulkan::VulkanRender&        m_render;
    SceneRenderPipelineOptions   m_options;
    RenderSceneSnapshot          m_snapshot;
    Option<Box<rg::RenderGraph>> m_graph;
    SceneRenderPipelineStats     m_stats;
};

} // namespace owe

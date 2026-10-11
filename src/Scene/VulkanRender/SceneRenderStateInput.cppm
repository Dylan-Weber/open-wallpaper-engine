export module wescene.vulkan_render:scene_render_state_input;
import rstd;
import vrento;
import :scene_render_resources;
import :scene_draw;
export import :scene_render_textures;

using namespace rstd::prelude;

export namespace owe::vulkan
{
struct SceneRenderDrawSource {
    fg::PassId            id;
    Box<SceneDraw>        source;
    PassInvalidationFlags invalidation { PassInvalidationNone };
};
struct SceneRenderStateInput {
    fg::FrameGraph                     graph;
    SceneRenderTextures                textures;
    Vec<SceneRenderDrawSource>         draws;
    Vec<vrento::vulkan::PassInput>     descriptions;
    fg::PassId                         output_pass, release_pass;
    fg::ResourceVersion                surface;
    Option<resource::TextureUseHandle> result_use;
    resource::ResourcePlan             resource_plan;
    ResourceDeclarationData            resource_data;
    Option<Arc<SceneImageSource>>      image_source;

    auto draw(fg::PassId id) const -> Option<ref<SceneDraw>> {
        for (const auto& entry : draws)
            if (entry.id == id) return Some(entry.source.as_ref());
        return None();
    }
};
} // namespace owe::vulkan

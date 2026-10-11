export module wescene.vulkan_render:scene_render_textures;
import rstd;
import vrento;
import :scene_render_resources;

using namespace rstd::prelude;
using namespace rstd::literals;

export namespace owe::vulkan
{
namespace fg                 = vrento::fg;
namespace resource           = vrento::resource;
using SceneRenderTextureSpec = vrento::pass::TextureSpec;
class SceneRenderTextures : public vrento::pass::GraphResources {
public:
    using vrento::pass::GraphResources::GraphResources;
    auto textureRequestDiagnostics(const fg::PassNode& pass) const
        -> Vec<PassTextureRequestDiagnostic> {
        Vec<PassTextureRequestDiagnostic> result;
        for (const auto& access : pass.uses) {
            auto texture = find(access.version.resource);
            if (! texture) continue;
            for (const auto& use : uses())
                if (use.version == access.version) {
                    result.push(
                        { .role    = access.access == fg::Access::Read ? "read"_Str : "write"_Str,
                          .name    = texture->spec.key.clone(),
                          .use     = Some(resource::TextureUseHandle(use.handle)),
                          .request = texture->spec.definition.valid()
                                         ? Some(texture->spec.request().clone())
                                         : None<resource::TextureRequest>() });
                    break;
                }
        }
        return result;
    }
};
} // namespace owe::vulkan

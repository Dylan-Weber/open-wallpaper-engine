export module wescene.vulkan_render:scene_render_resources;
import rstd;
import wescene.vulkan;
import wescene.scene;

import :resource;
import :pipeline_layout;
import :shader_reflection_cache;
import :uniform_buffer;

using namespace rstd::prelude;
using rstd::collections::HashMap;
using rstd::sync::Arc;

export namespace owe
{

namespace vulkan
{

using PassInvalidationFlags = u32;

enum class PassInvalidation : rstd::uint32_t
{
    Content     = 1u << 3,
    Resources   = 1u << 0u,
    Pipeline    = 1u << 1u,
    Framebuffer = 1u << 2u,
};

inline constexpr PassInvalidationFlags ToPassInvalidationFlags(PassInvalidation invalidation) {
    return PassInvalidationFlags(static_cast<rstd::uint32_t>(invalidation));
}

inline constexpr PassInvalidationFlags PassInvalidationNone {};
inline constexpr PassInvalidationFlags PassInvalidationAll =
    ToPassInvalidationFlags(PassInvalidation::Resources) |
    ToPassInvalidationFlags(PassInvalidation::Pipeline) |
    ToPassInvalidationFlags(PassInvalidation::Framebuffer);

struct MaterialTextureBindingRefresh {
    PassInvalidationFlags invalidation_flags { PassInvalidationNone };
    bool                  requires_graph_rebuild { false };
};

struct PassTextureRequestDiagnostic {
    String                             role;
    u32                                slot { 0 };
    String                             name;
    Option<resource::TextureUseHandle> use;
    Option<TextureRequest>             request;
};

struct PassResourceUses {
    Vec<resource::TextureUseHandle>        textures;
    Vec<resource::BufferUseHandle>         buffers;
    Vec<resource::ShaderUseHandle>         shaders;
    Vec<resource::PipelineUseHandle>       pipelines;
    Vec<resource::RenderPassUseHandle>     render_passes;
    Vec<resource::FramebufferUseHandle>    framebuffers;
    Vec<resource::DescriptorBindingHandle> descriptors;
    Vec<resource::ExternalUseHandle>       externals;

    friend bool operator==(const PassResourceUses&, const PassResourceUses&) = default;
};

struct GlobalDescriptorBufferUse {
    u32                       binding {};
    u64                       identity {};
    resource::BufferUseHandle buffer;
};

class PreparedPassResources {
public:
    PreparedPassResources(const resource_registry::PreparedResourceTable& prepared,
                          const PassResourceUses&                         uses)
        : m_prepared(ref<resource_registry::PreparedResourceTable>::from_raw_parts(
              rstd::addressof(prepared))),
          m_uses(ref<PassResourceUses>::from_raw_parts(rstd::addressof(uses))) {}

    auto Resolve(resource::TextureUseHandle use) const
        -> Option<ref<resource_registry::PreparedTexture>> {
        return Contains(m_uses->textures, use) ? m_prepared->Resolve(use) : None();
    }

    auto Resolve(resource::BufferUseHandle use) const
        -> Option<ref<resource_registry::PreparedBufferUse>> {
        return Contains(m_uses->buffers, use) ? m_prepared->Resolve(use) : None();
    }

    auto Resolve(resource::ShaderUseHandle use) const
        -> Option<ref<resource_registry::PreparedShaderUse>> {
        return Contains(m_uses->shaders, use) ? m_prepared->Resolve(use) : None();
    }

    auto Resolve(resource::PipelineUseHandle use) const
        -> Option<ref<resource_registry::PreparedPipeline>> {
        return Contains(m_uses->pipelines, use) ? m_prepared->Resolve(use) : None();
    }

    auto Resolve(resource::RenderPassUseHandle use) const
        -> Option<ref<resource_registry::PreparedRenderPass>> {
        return Contains(m_uses->render_passes, use) ? m_prepared->Resolve(use) : None();
    }

    auto Resolve(resource::FramebufferUseHandle use) const
        -> Option<ref<resource_registry::PreparedFramebuffer>> {
        return Contains(m_uses->framebuffers, use) ? m_prepared->Resolve(use) : None();
    }

    auto Resolve(resource::DescriptorBindingHandle handle) const
        -> Option<ref<resource_registry::PreparedDescriptorBinding>> {
        return Contains(m_uses->descriptors, handle) ? m_prepared->Resolve(handle) : None();
    }

    auto Resolve(resource::ExternalUseHandle use) const
        -> Option<ref<resource_registry::PreparedExternalUse>> {
        return Contains(m_uses->externals, use) ? m_prepared->Resolve(use) : None();
    }

private:
    template<typename Handle>
    static bool Contains(const Vec<Handle>& handles, Handle value) {
        for (usize index {}; index < handles.len(); ++index) {
            if (handles[index] == value) return true;
        }
        return false;
    }

    ref<resource_registry::PreparedResourceTable> m_prepared;
    ref<PassResourceUses>                         m_uses;
};

struct DrawBindingContext {
    ref<PreparedPassResources>                                resources;
    Option<ref<resource_registry::PreparedDescriptorBinding>> global_descriptor;
    vrento::vulkan::DeviceSession                             session;
};

struct DrawUpdateContext {
    mut_ref<dyn<resource::BufferContentWriter>>               buffers;
    ref<PreparedPassResources>                                resources;
    mut_ref<dyn<resource_registry::GraphicsResourcePreparer>> graphics;
    ref<dyn<SceneTextureAnimationView>>                       textures;
};

struct DrawResourceContext {
    ref<dyn<ShaderBackend>>                                   shader_backend;
    ref<resource_registry::PreparedResourceTable>             resources;
    mut_ref<dyn<resource_registry::GraphicsResourcePreparer>> graphics;
    ref<PipelineLayoutAssignments>                            pipeline_layouts;
};

struct ResourceDeclarationData {
    u64                                                        m_next_buffer { 0 };
    u64                                                        m_next_shader { 0 };
    u64                                                        m_next_pipeline { 0 };
    u64                                                        m_next_render_pass { 0 };
    u64                                                        m_next_framebuffer { 0 };
    u64                                                        m_next_external { 0 };
    HashMap<String, Vec<rstd::u8>>                             m_buffers;
    HashMap<String, resource::BufferUseHandle>                 m_shared_buffers;
    HashMap<resource::ShaderRequest, resource::ShaderArtifact> m_shaders;
};

class ResourceDeclarationContext {
public:
    explicit ResourceDeclarationContext(resource::ResourcePlan& plan,
                                        ShaderReflectionCache&  shader_cache,
                                        ResourceDeclarationData data = {})
        : m_plan(plan),
          m_shader_cache(
              mut_ref<ShaderReflectionCache>::from_raw_parts(rstd::addressof(shader_cache))),
          data_(rstd::move(data)) {}

    auto takeData() -> ResourceDeclarationData { return rstd::move(data_); }

    auto AddBuffer(resource::BufferRequest request, slice<rstd::u8> content)
        -> resource::BufferUseHandle {
        auto bytes = Vec<rstd::u8>::from(content);
        return AddBuffer(rstd::move(request), rstd::move(bytes));
    }

    auto AddBuffer(resource::BufferRequest request) -> resource::BufferUseHandle {
        auto bytes = Vec<rstd::u8>::with_capacity(request.definition.size);
        bytes.resize(request.definition.size, rstd::u8(0));
        return AddBuffer(rstd::move(request), rstd::move(bytes));
    }

    auto AddSharedBuffer(resource::BufferRequest request) -> resource::BufferUseHandle {
        auto existing = data_.m_shared_buffers.get(request.name);
        if (existing.is_some()) return **existing;
        auto name   = request.name.clone();
        auto handle = AddBuffer(rstd::move(request));
        if (handle.Valid()) (void)data_.m_shared_buffers.insert(rstd::move(name), handle);
        return handle;
    }

    auto AddTexture(resource::TextureRequest request, resource::ResourceAccess access)
        -> resource::TextureUseHandle {
        return m_plan.DeclareTexture(rstd::move(request), access);
    }

    auto AddShader(resource::ShaderRequest request, const SceneShader& shader)
        -> resource::ShaderUseHandle {
        auto handle = resource::ShaderUseHandle {
            .index      = data_.m_next_shader++,
            .generation = m_plan.generation,
        };
        SceneShaderArtifactProvider provider(*m_shader_cache, shader);
        auto                        artifact = provider.LoadShader(request);
        if (artifact.is_err()) return {};
        auto artifact_key = request.clone();
        (void)data_.m_shaders.insert(rstd::move(artifact_key),
                                     rstd::move(artifact).unwrap_unchecked());
        m_plan.shaders.push(resource::ShaderPlanEntry {
            .handle  = handle,
            .request = rstd::move(request),
        });
        return handle;
    }

    auto ReservePipeline() -> resource::PipelineUseHandle {
        return resource::PipelineUseHandle {
            .index      = data_.m_next_pipeline++,
            .generation = m_plan.generation,
        };
    }

    auto ReserveRenderPass() -> resource::RenderPassUseHandle {
        return resource::RenderPassUseHandle {
            .index      = data_.m_next_render_pass++,
            .generation = m_plan.generation,
        };
    }

    auto ReserveFramebuffer() -> resource::FramebufferUseHandle {
        return resource::FramebufferUseHandle {
            .index      = data_.m_next_framebuffer++,
            .generation = m_plan.generation,
        };
    }

    auto ReserveExternal() -> resource::ExternalUseHandle {
        return resource::ExternalUseHandle {
            .index      = data_.m_next_external++,
            .generation = m_plan.generation,
        };
    }

    auto LoadBuffer(const resource::BufferRequest& request)
        -> Result<slice<rstd::u8>, resource::ResourceError> {
        auto content = data_.m_buffers.get(request.name);
        if (content.is_none()) {
            return Err(resource::ResourceError {
                .kind    = resource::ResourceErrorKind::MissingContent,
                .message = rstd::format("buffer content {} unavailable", request.name),
            });
        }
        return Ok((**content).as_slice());
    }

    auto ShaderArtifact(const resource::ShaderRequest& request) const
        -> Option<ref<resource::ShaderArtifact>> {
        return data_.m_shaders.get(request);
    }

    auto ScopeResourceName(String local_name) const -> String {
        return rstd::format("plan:{}:{}", m_plan.generation, local_name);
    }

private:
    auto AddBuffer(resource::BufferRequest request, Vec<rstd::u8> content)
        -> resource::BufferUseHandle {
        auto handle = resource::BufferUseHandle {
            .index      = data_.m_next_buffer++,
            .generation = m_plan.generation,
        };
        auto name = request.name.clone();
        (void)data_.m_buffers.insert(rstd::move(name), rstd::move(content));
        m_plan.buffers.push(resource::BufferPlanEntry {
            .handle     = handle,
            .definition = rstd::move(request),
        });
        return handle;
    }
    resource::ResourcePlan&        m_plan;
    mut_ref<ShaderReflectionCache> m_shader_cache;
    ResourceDeclarationData        data_;
};

struct PassTextureAccess {
    resource::TextureUseHandle        use;
    vrento::fg::Access                access;
    Option<vrento::fg::ResourceRange> range;
};

} // namespace vulkan
} // namespace owe

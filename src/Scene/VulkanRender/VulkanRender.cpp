module;
#include <rstd/macro.hpp>

#include "vvk/macros.hpp"

#include <cerrno>
#include <unistd.h>

module wescene.vulkan_render;
import vvk;
import wescene.core;
import wescene.types;
import rstd.log;
import wescene.load_bench;
import wescene.resource_registry;
import wescene.vulkan;
import wescene.utils;
import wescene.scene;
import wescene.text;

using rstd::time::Duration;

using rstd::mem::memcpy;
using rstd::mem::memset;

using namespace owe::vulkan;
using namespace rstd::prelude;
using namespace rstd::literals;
using rstd::ffi::CStr;
using rstd::sync::Arc;
using rstd::sync::atomic::Atomic;
using rstd::sync::atomic::Ordering;

constexpr rstd::uint64_t vk_wait_time { static_cast<rstd::uint64_t>(
    Duration::from_secs(u64(10)).as_nanos().to_primitive()) };
struct FrameSynchronizationOwner {
    vrento::vulkan::DeviceSession session;
    Arc<vvk::Semaphore>           acquire;
    Arc<vvk::Semaphore>           signal;
    Option<owe::ExSwapchainOwner> output;
};

struct HostOutputFrame {
    vrento::vulkan::SurfaceTarget                 target;
    Arc<FrameSynchronizationOwner>                synchronization;
    Option<owe::FrameSurfaceCompletionCapability> completion;
    Option<rstd::uint32_t>                        window_image;
    bool                                          blocked {};
};

struct ViewerSurfaceFactory {
    owe::CreateSurfaceCallback create;
    auto                       createSurface(VkInstance instance)
        -> Result<vrento::vulkan::SessionSurface, vrento::vulkan::DeviceSessionError> {
        VkSurfaceKHR surface {};
        const auto   result = create->call_once(instance, &surface);
        if (result != VK_SUCCESS)
            return Err(vrento::vulkan::DeviceSessionError {
                vrento::vulkan::DeviceSessionStage::Surface, result });
        return Ok(vrento::vulkan::SessionSurface {
            .handle   = surface,
            .lifetime = Box<dyn<rstd::any::Any>>::make(rstd::move(create)),
        });
    }
};

using namespace owe;

namespace rstd
{
template<>
struct Impl<vrento::vulkan::SessionSurfaceFactory, ViewerSurfaceFactory>
    : ImplBase<ViewerSurfaceFactory> {
    auto create(VkInstance instance)
        -> Result<vrento::vulkan::SessionSurface, vrento::vulkan::DeviceSessionError> {
        return self().createSurface(instance);
    }
};
} // namespace rstd

bool SameRenderItemId(owe::RenderItemId lhs, owe::RenderItemId rhs) {
    return lhs.index == rhs.index && lhs.generation == rhs.generation;
}

void PushUniqueRenderItem(Vec<owe::RenderItemId>& items, owe::RenderItemId id) {
    for (usize index {}; index < items.len(); ++index) {
        if (SameRenderItemId(items[index], id)) return;
    }
    items.push(rstd::move(id));
}

Vec<owe::RenderItemId> RenderItemsForMaterials(const owe::RenderSceneSnapshot& render_scene,
                                               slice<owe::SceneMaterialId>     materials) {
    Vec<owe::RenderItemId> render_items;
    for (usize material_index {}; material_index < materials.len(); ++material_index) {
        auto material       = materials[material_index];
        auto material_items = render_scene.renderItemsFor(material);
        for (usize index {}; index < material_items.len(); ++index) {
            PushUniqueRenderItem(render_items, material_items[index]);
        }
    }
    return render_items;
}

const rstd::array<Extension, 4> base_inst_exts {
    Extension {
        false,
        CStr::from_ptr(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME).to_str().unwrap() },
    Extension {
        false,
        CStr::from_ptr(VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME).to_str().unwrap() },
    Extension {
        false,
        CStr::from_ptr(VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME).to_str().unwrap() },
    Extension {
        false,
        CStr::from_ptr(VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME).to_str().unwrap() },
};
#if __is_target_os(macos)
const rstd::array<Extension, 8> base_device_exts {
    Extension { false, CStr::from_ptr(VK_EXT_DEPTH_CLIP_ENABLE_EXTENSION_NAME).to_str().unwrap() },
    // MoltenVK's swapchain→Metal present path uses the portability subset.
    Extension { false, "VK_KHR_portability_subset"_str },
    Extension { false, CStr::from_ptr(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME).to_str().unwrap() },
    // VideoToolbox frames are imported through MoltenVK's Metal-object bridge.
    Extension { false, "VK_EXT_metal_objects"_str },
    Extension { false, CStr::from_ptr(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME).to_str().unwrap() },
    Extension {
        false,
        CStr::from_ptr(VK_EXT_SHADER_VIEWPORT_INDEX_LAYER_EXTENSION_NAME).to_str().unwrap() },
    Extension { true, CStr::from_ptr(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME).to_str().unwrap() },
    Extension { false, CStr::from_ptr(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME).to_str().unwrap() },
};
#else
const rstd::array<Extension, 9> base_device_exts {
    Extension { false, CStr::from_ptr(VK_EXT_DEPTH_CLIP_ENABLE_EXTENSION_NAME).to_str().unwrap() },
    Extension { false, CStr::from_ptr(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME).to_str().unwrap() },
    Extension { false, CStr::from_ptr(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME).to_str().unwrap() },
    Extension {
        false,
        CStr::from_ptr(VK_EXT_SHADER_VIEWPORT_INDEX_LAYER_EXTENSION_NAME).to_str().unwrap() },
    Extension { true, CStr::from_ptr(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME).to_str().unwrap() },
    Extension { true,
                CStr::from_ptr(VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME).to_str().unwrap() },
    Extension { true, CStr::from_ptr(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME).to_str().unwrap() },
    Extension { false, CStr::from_ptr(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME).to_str().unwrap() },
    // Optional. When present we can report the picked physical device's
    // DRM render-node major/minor via `getDrmRenderNode()` so the
    // waywallen daemon can match it against each connected display's
    // GPU. When absent the accessor returns false and callers report
    // (0, 0); the daemon then conservatively assumes cross-GPU.
    Extension { false,
                CStr::from_ptr(VK_EXT_PHYSICAL_DEVICE_DRM_EXTENSION_NAME).to_str().unwrap() },
};
#endif

void AppendVideoDeviceExtensions(Vec<Extension>& device_exts) {
#if __is_target_os(macos)
    // VideoToolbox frames use the Metal-object import path. The DMA-BUF/DRM
    // and Vulkan-video extensions below are Linux-only interop requirements.
    (void)device_exts;
#else
    device_exts.push(
        { false, CStr::from_ptr(VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME).to_str().unwrap() });
    device_exts.push(
        { false, CStr::from_ptr(VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME).to_str().unwrap() });
    device_exts.push(
        { false,
          CStr::from_ptr(VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME).to_str().unwrap() });
    device_exts.push(
        { false, CStr::from_ptr(VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME).to_str().unwrap() });
    device_exts.push(
        { false, CStr::from_ptr(VK_KHR_VIDEO_QUEUE_EXTENSION_NAME).to_str().unwrap() });
    device_exts.push(
        { false, CStr::from_ptr(VK_KHR_VIDEO_DECODE_QUEUE_EXTENSION_NAME).to_str().unwrap() });
    device_exts.push(
        { false, CStr::from_ptr(VK_KHR_VIDEO_DECODE_H264_EXTENSION_NAME).to_str().unwrap() });
    device_exts.push(
        { false, CStr::from_ptr(VK_KHR_VIDEO_DECODE_H265_EXTENSION_NAME).to_str().unwrap() });
    device_exts.push(
        { false, CStr::from_ptr(VK_KHR_VIDEO_DECODE_AV1_EXTENSION_NAME).to_str().unwrap() });
    device_exts.push(
        { false, CStr::from_ptr(VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME).to_str().unwrap() });
    device_exts.push(
        { false, CStr::from_ptr(VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME).to_str().unwrap() });
    device_exts.push(
        { false, CStr::from_ptr(VK_EXT_SHADER_OBJECT_EXTENSION_NAME).to_str().unwrap() });
#endif
}

void ReleaseCompletedRetiredResources(const Device& device, RenderingResources& rr) {
    auto memory = rstd::dyn<owe::vulkan::MemoryBudgetSource>::from_ref(device);
    rr.resources.Collect(memory.as_ref());
}

enum class SceneRenderPreparationProgress
{
    BatchReady,
    Complete,
    Failed,
    Pending
};

auto SceneCompileProgress(Result<empty, vrento::vulkan::GraphExecutionError> result)
    -> SceneRenderPreparationProgress {
    if (result.is_ok()) return SceneRenderPreparationProgress::Complete;
    const auto error = result.unwrap_err();
    if (error.stage == vrento::vulkan::GraphExecutionStage::Pending)
        return SceneRenderPreparationProgress::Pending;
    rstd_error("scene compile failed: stage={} pass={} resource={} native={}",
               static_cast<unsigned>(error.stage),
               error.pass.value,
               error.resource.value,
               error.native);
    if (error.graph)
        rstd_error("scene graph failure: kind={}", static_cast<unsigned>(error.graph->kind));
    return SceneRenderPreparationProgress::Failed;
}

struct VulkanRender::Impl {
    Impl() = default;
    ~Impl() { destroy(); }

    bool init(RenderInitInfo, SceneLoadBenchRecorderView);
    void destroy();

    void drawFrame(Scene&);
    auto preparationReady() -> Result<empty, RenderPrepareError>;

    bool CreateRenderingResource(RenderingResources&);
    void DestroyRenderingResource(RenderingResources&);

    void clearLastRenderGraph(RenderGraphResourceRetention);
    void configureRenderTargets(Scene&);
    auto installSceneRenderState(Scene&, vulkan::SceneRenderStateInput&&,
                                 const RenderSceneSnapshot&, SceneLoadBenchRecorderView)
        -> Result<empty, RenderPrepareError>;
    void invalidatePreparedRenderItems(slice<owe::RenderItemId>, PassInvalidationFlags);
    Vec<PreparedPassDiagnostic> preparedPassDiagnostics() const;
    void                        UpdateCameraFillMode(Scene&, owe::FillMode);

    bool initRes();
    auto prepareSceneRenderState(Scene&, const RenderSceneSnapshot&, resource::ResourcePlanSections,
                                 SceneLoadBenchRecorderView = {})
        -> Result<empty, RenderPrepareError>;
    auto finishSceneRenderPreparation(Scene&, SceneRenderPreparationProgress,
                                      SceneLoadBenchRecorderView = {})
        -> Result<empty, RenderPrepareError>;
    bool sceneRenderReady() const;
    void abortSceneRenderPreparation();
    auto continueSceneRenderPreparation(Scene&, SceneLoadBenchRecorderView = {})
        -> SceneRenderPreparationProgress;
    Option<Arc<vrento::vulkan::PreparedGraphicsResources>> m_graphics;
    vrento::vulkan::TextureOutput                          m_texture_output;
    VkFormat                                               m_present_format { VK_FORMAT_UNDEFINED };
    bool                                        waitForPreparedUploads(RenderingResources&);
    Option<vrento::vulkan::GraphFrame>          m_frame;
    Option<vrento::vulkan::GraphExecutionError> m_last_frame_error;
    Option<HostOutputFrame>                     m_output;
    bool                                        m_frame_submitted {};
    bool prepareFontUploads(vrento::texture::Submission&,
                            Vec<vrento::resource_registry::PreparedTextureUpdate>&);
    bool beginFrame(vrento::texture::Submission);
    bool executeFrame();
    void reportFrameError(vrento::vulkan::GraphExecutionError);
    void failOutput(bool uncertain = false);
    auto createFrameSynchronization(bool external) -> Option<Arc<FrameSynchronizationOwner>>;
    void closeOutput();
    void ackFontUploads();
    void drawFrameSwapchain(Scene&);
    void drawFrameOffscreen(Scene&);
    bool onSwapchainReady(unsigned width, unsigned height);

    Option<vrento::vulkan::DeviceSession> m_session;
    Option<vrento::vulkan::Renderer>      m_renderer;
    const Device*                         m_device {};
    VkExtent2D                            m_extent {};
    ReDrawCB                              m_redraw_cb;

    Box<dyn<ShaderBackend>>    m_shader_backend { MakeShaderBackend() };
    ShaderReflectionCache      m_shader_reflection_cache { m_shader_backend.as_ref() };
    SceneLoadBenchRecorderView m_pending_load_bench;

    bool m_with_surface { false };
    bool m_inited { false };

    // MSAA sample count for the screen RT only. 1bit = disabled.
    // Resolved against device's framebufferColorSampleCounts in init().
    VkSampleCountFlagBits m_msaa_samples { VK_SAMPLE_COUNT_1_BIT };

    Option<ExSwapchainOwner> m_ex_swapchain;
    RenderingResources       m_rendering_resources;
    u64                      m_next_surface_acquire_serial { 1 };

    // for VUID-vkQueueSubmit-pSignalSemaphores-00067
    Vec<Arc<vvk::Semaphore>> m_sem_swap_finish_per_image;

    SceneRenderState m_scene_render_state;

    struct FontUpdate {
        rstd::sync::Arc<owe::text::AtlasPage> page;
        owe::text::AtlasUpdateBatch           batch;
        bool                                  recorded {};
    };
    Vec<FontUpdate> m_pending_font_updates;
};

VulkanRender::VulkanRender(): pImpl(Box<Impl>::make()) {}
VulkanRender::~VulkanRender() = default;

bool VulkanRender::inited() const { return pImpl->m_inited; }

int VulkanRender::takeLastFrameSyncFd() {
    return pImpl->m_ex_swapchain ? (*pImpl->m_ex_swapchain)->AsSwapchain().takeLastFrameSyncFd()
                                 : -1;
}

bool VulkanRender::getDrmRenderNode(rstd::uint32_t& out_major, rstd::uint32_t& out_minor) const {
    if (! pImpl->m_inited) return false;
    VkPhysicalDeviceDrmPropertiesEXT drm {};
    drm.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRM_PROPERTIES_EXT;
    VkPhysicalDeviceProperties2KHR props {};
    props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
    props.pNext = &drm;
    pImpl->m_device->gpu().GetProperties2KHR(props);
    if (! drm.hasRender) return false;
    if (drm.renderMajor < 0 || drm.renderMinor < 0 ||
        static_cast<rstd::uint64_t>(drm.renderMajor) > u32::MAX.to_primitive() ||
        static_cast<rstd::uint64_t>(drm.renderMinor) > u32::MAX.to_primitive()) {
        return false;
    }
    out_major = static_cast<rstd::uint32_t>(drm.renderMajor);
    out_minor = static_cast<rstd::uint32_t>(drm.renderMinor);
    return true;
}

DeviceCapabilities VulkanRender::deviceCapabilities() const {
    if (! pImpl->m_inited) return {};
    return pImpl->m_device->capabilities();
}

VkInstance VulkanRender::vkInstance() const {
    if (! pImpl->m_inited) return VK_NULL_HANDLE;
    return pImpl->m_session->instance();
}

VkPhysicalDevice VulkanRender::vkPhysicalDevice() const {
    if (! pImpl->m_inited) return VK_NULL_HANDLE;
    return *pImpl->m_device->gpu();
}

VkDevice VulkanRender::vkDevice() const {
    if (! pImpl->m_inited) return VK_NULL_HANDLE;
    return *pImpl->m_device->handle();
}

VkQueue VulkanRender::vkGraphicsQueue() const {
    if (! pImpl->m_inited) return VK_NULL_HANDLE;
    return *pImpl->m_device->graphics_queue().handle;
}

rstd::uint32_t VulkanRender::vkGraphicsQueueFamily() const {
    if (! pImpl->m_inited) return 0;
    return pImpl->m_device->graphics_queue().family_index;
}

void VulkanRender::deviceUuid(rstd::uint8_t out[16]) const {
    memset(out, rstd::u8(0), rstd::usize(16));
    if (! pImpl->m_inited) return;
    VkPhysicalDeviceIDPropertiesKHR id {};
    id.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES_KHR;
    VkPhysicalDeviceProperties2KHR props {};
    props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
    props.pNext = &id;
    pImpl->m_device->gpu().GetProperties2KHR(props);
    memcpy(out, id.deviceUUID, rstd::usize(16));
}

void VulkanRender::pumpVideoTextures(double dt_seconds) { (void)tryPumpVideoTextures(dt_seconds); }

auto VulkanRender::tryPumpVideoTextures(double dt_seconds) -> Result<empty, RenderPrepareError> {
    auto ready = pImpl->preparationReady();
    if (ready.is_err()) return ready;
    if (! pImpl->m_rendering_resources.resources.PumpVideoTextures(dt_seconds))
        return Err(RenderPrepareError { RenderPrepareStage::Upload });
    return Ok(empty {});
}

void VulkanRender::pumpFontAtlases(Scene& scene) { (void)tryPumpFontAtlases(scene); }

auto VulkanRender::tryPumpFontAtlases(Scene& scene) -> Result<empty, RenderPrepareError> {
    auto ready = pImpl->preparationReady();
    if (ready.is_err()) return ready;
    pImpl->m_pending_font_updates.clear();
    auto* fonts = owe::text::SceneTextResources(scene);
    if (fonts == nullptr) return Ok(empty {});
    for (auto& page : fonts->Pages()) {
        auto batch = page->SnapshotDirtyUpdates();
        if (batch.rects.is_empty()) continue;
        pImpl->m_pending_font_updates.push(Impl::FontUpdate { page.clone(), rstd::move(batch) });
    }
    return Ok(empty {});
}

void VulkanRender::Impl::ackFontUploads() {
    m_pending_font_updates.retain([](const FontUpdate& update) {
        if (! update.recorded) return true;
        update.page->AcknowledgeUpdates(update.batch.serial);
        return false;
    });
}

bool VulkanRender::Impl::prepareFontUploads(
    vrento::texture::Submission&                           work,
    Vec<vrento::resource_registry::PreparedTextureUpdate>& uploads) {
    for (auto& update : m_pending_font_updates) {
        update.recorded = false;
        auto definition = m_scene_render_state.textureResource(update.page->AtlasUrl());
        if (! definition) continue;
        bool resident = true;
        for (const auto& region : update.batch.rects) {
            vrento::texture::TextureUpload upload;
            upload.x         = region.rect.x;
            upload.y         = region.rect.y;
            upload.width     = region.rect.w;
            upload.height    = region.rect.h;
            upload.row_pitch = region.rect.w;
            upload.bytes     = region.bytes.clone();
            auto prepared =
                m_rendering_resources.resources.PrepareRegion(*definition, rstd::move(upload));
            if (prepared.is_err()) return false;
            auto packet = prepared.unwrap();
            if (! packet) {
                resident = false;
                break;
            }
            uploads.push(packet.take().unwrap());
        }
        update.recorded = resident;
        if (resident) work.retain(update.page.clone());
    }
    return true;
}

auto VulkanRender::Impl::createFrameSynchronization(bool external)
    -> Option<Arc<FrameSynchronizationOwner>> {
    auto                  owner = Arc<FrameSynchronizationOwner>::make(FrameSynchronizationOwner {
        *m_session,
        Arc<vvk::Semaphore>::make(),
        Arc<vvk::Semaphore>::make(),
        external ? Some(m_ex_swapchain->clone()) : None<ExSwapchainOwner>() });
    VkSemaphoreCreateInfo info { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkExportSemaphoreCreateInfo exported { VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO };
    exported.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;
    if (external && m_device->handle().Dispatch().vkGetSemaphoreFdKHR) info.pNext = &exported;
    auto result =
        m_device->handle().CreateSemaphore(info, external ? *owner->signal : *owner->acquire);
    if (result != VK_SUCCESS) return None();
    return Some(rstd::move(owner));
}
bool VulkanRender::Impl::beginFrame(vrento::texture::Submission work) {
    Vec<vrento::resource_registry::PreparedTextureUpdate> uploads;
    if (! prepareFontUploads(work, uploads)) return false;
    auto request = m_scene_render_state.captureFrame(m_rendering_resources,
                                                     m_renderer->compiled().unwrap(),
                                                     m_texture_output,
                                                     m_output->target,
                                                     (*m_graphics)->shared_descriptor());
    if (request.is_err()) {
        reportFrameError(request.unwrap_err());
        return false;
    }
    auto& sync = *m_output->synchronization;
    if (! m_rendering_resources.resources.AttachSignal(
            work,
            vrento::vulkan::NativeSemaphoreSignal { .device    = *m_device->handle(),
                                                    .semaphore = **sync.signal,
                                                    .owner     = m_output->target.owner() }))
        return false;
    auto frame = vrento::vulkan::GraphFrame::begin(m_rendering_resources.resource_owner.clone(),
                                                   m_renderer->compiled().unwrap(),
                                                   request.unwrap(),
                                                   rstd::move(work),
                                                   rstd::move(uploads));
    if (frame.is_err()) {
        reportFrameError(frame.unwrap_err());
        return false;
    }
    m_frame            = Some(frame.unwrap());
    m_last_frame_error = None();
    return true;
}
void VulkanRender::Impl::reportFrameError(vrento::vulkan::GraphExecutionError error) {
    m_last_frame_error = Some(error);
    rstd_error("frame failed: stage={} pass={} resource={} native={}",
               static_cast<unsigned>(error.stage),
               error.pass.value,
               error.resource.value,
               error.native);
    if (error.texture)
        rstd_error("texture failure: kind={} native={}",
                   static_cast<unsigned>(error.texture->kind),
                   error.texture->native);
    if (m_scene_render_state.installed())
        if (auto resource = m_scene_render_state.inputGraph().find_resource(error.resource))
            rstd_error("frame resource: {}", resource->name);
}
void VulkanRender::Impl::failOutput(bool uncertain) {
    if (m_frame && m_frame->error()) reportFrameError(*m_frame->error());
    if (m_output && (uncertain || m_output->window_image))
        m_output->blocked = true;
    else {
        m_frame  = None();
        m_output = None();
    }
    m_renderer->clear();
}
bool VulkanRender::Impl::executeFrame() {
    using vrento::vulkan::FrameState;
    if (! m_frame || ! m_output || m_output->blocked) return false;
    if (m_frame->state() == FrameState::Pending) {
        auto prepared = m_frame->advance();
        if (prepared.is_err()) {
            if (prepared.unwrap_err().stage != vrento::vulkan::GraphExecutionStage::Pending)
                failOutput();
            return false;
        }
    }
    if (m_frame->state() == FrameState::Ready && m_frame->record().is_err()) {
        failOutput();
        return false;
    }
    if (m_frame->state() == FrameState::Recorded) {
        auto submitted = m_frame->submit();
        if (submitted.is_err()) {
            failOutput(m_frame->receipt().uncertain() || m_frame->receipt().submitted());
            return false;
        }
        m_frame_submitted = true;
        ackFontUploads();
    }
    if (m_frame->state() == FrameState::Submitted) {
        auto waited = m_frame->wait(vk_wait_time);
        if (waited.is_err()) {
            if (waited.unwrap_err().kind != vrento::texture::ErrorKind::Timeout) {
                m_output->blocked = true;
                rstd_error("frame completion wait failed: {}", waited.unwrap_err().native);
            }
            return false;
        }
    }
    return m_frame->state() == FrameState::Completed;
}
void VulkanRender::Impl::closeOutput() {
    const bool submitted =
        m_frame && (m_frame->receipt().submitted() || m_frame->receipt().uncertain());
    if (m_frame && ! submitted) m_frame->cancel();
    if (m_output && m_output->window_image && ! submitted && ! m_session->lost()) {
        auto& queue   = m_rendering_resources.resources.TextureContext();
        auto  started = queue.Begin();
        if (started.is_err()) rstd::panic("cannot drain acquired window frame");
        auto work     = started.unwrap();
        auto attached = queue.AttachWait(
            work,
            vrento::vulkan::NativeSemaphoreWait { .device    = *m_device->handle(),
                                                  .semaphore = **m_output->synchronization->acquire,
                                                  .owner     = m_output->target.owner() });
        if (attached.is_err()) rstd::panic("cannot retain window acquisition");
        auto drained = queue.Submit(rstd::move(work));
        if (drained.is_err()) {
            if (! m_session->lost()) rstd::panic("cannot submit window acquisition drain");
        } else {
            auto waited = queue.wait(drained.unwrap(), ~rstd::uint64_t());
            if (waited.is_err() && ! m_session->lost())
                rstd::panic("cannot finish window acquisition drain");
        }
    }
    auto idle = m_session->wait_idle();
    if (idle != VK_SUCCESS && idle != VK_ERROR_DEVICE_LOST)
        rstd::panic("cannot retire output resources");
    m_frame  = None();
    m_output = None();
}

void VulkanRender::driverUuid(rstd::uint8_t out[16]) const {
    memset(out, rstd::u8(0), rstd::usize(16));
    if (! pImpl->m_inited) return;
    VkPhysicalDeviceIDPropertiesKHR id {};
    id.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES_KHR;
    VkPhysicalDeviceProperties2KHR props {};
    props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
    props.pNext = &id;
    pImpl->m_device->gpu().GetProperties2KHR(props);
    memcpy(out, id.driverUUID, rstd::usize(16));
}

bool VulkanRender::init(RenderInitInfo info, SceneLoadBenchRecorderView load_bench) {
    return pImpl->init(rstd::move(info), load_bench);
}
void VulkanRender::destroy() { pImpl->destroy(); }
void VulkanRender::drawFrame(Scene& scene) { pImpl->drawFrame(scene); }
bool VulkanRender::framePending() const {
    return (pImpl->m_frame.is_some() || pImpl->m_output.is_some());
}
bool VulkanRender::frameSubmitted() const { return pImpl->m_frame_submitted; }
void VulkanRender::clearLastRenderGraph(RenderGraphResourceRetention retention) {
    (void)tryClearLastRenderGraph(retention);
}

auto VulkanRender::Impl::preparationReady() -> Result<empty, RenderPrepareError> {
    if (! m_inited) return Err(RenderPrepareError { RenderPrepareStage::State });
    if (m_frame || m_output || m_renderer->pending())
        return Err(RenderPrepareError { RenderPrepareStage::Pending });
    if (! waitForPreparedUploads(m_rendering_resources))
        return Err(RenderPrepareError { RenderPrepareStage::Upload });
    return Ok(empty {});
}
auto VulkanRender::tryContinuePrepare(Scene& scene) -> Result<empty, RenderPrepareError> {
    if (! pImpl->m_inited) return Err(RenderPrepareError { RenderPrepareStage::State });
    if (! pImpl->m_renderer->pending()) {
        if (pImpl->sceneRenderReady()) return Ok(empty {});
        return Err(RenderPrepareError { RenderPrepareStage::State });
    }
    auto status = pImpl->continueSceneRenderPreparation(scene);
    return pImpl->finishSceneRenderPreparation(scene, status);
}
auto VulkanRender::deviceSession() const
    -> Result<vrento::vulkan::DeviceSession, RenderPrepareError> {
    if (! pImpl->m_session) return Err(RenderPrepareError { RenderPrepareStage::State });
    return Ok(vrento::vulkan::DeviceSession(*pImpl->m_session));
}
auto VulkanRender::renderedTarget(ref<str> name)
    -> Result<vrento::texture::TextureViewRef, RenderPrepareError> {
    auto definition = pImpl->m_scene_render_state.textureResource(name);
    if (! definition) return Err(RenderPrepareError { RenderPrepareStage::State });
    auto result = pImpl->m_rendering_resources.resources.RenderedTarget(*definition);
    if (result.is_err())
        return Err(
            RenderPrepareError { result.unwrap_err().kind == resource::ResourceErrorKind::Pending
                                     ? RenderPrepareStage::Pending
                                     : RenderPrepareStage::State });
    return Ok(result.unwrap());
}

auto VulkanRender::tryClearLastRenderGraph(RenderGraphResourceRetention retention)
    -> Result<empty, RenderPrepareError> {
    if (! pImpl->m_inited) return Err(RenderPrepareError { RenderPrepareStage::State });
    if (pImpl->m_frame) return Err(RenderPrepareError { RenderPrepareStage::Pending });
    if (pImpl->m_output) return Err(RenderPrepareError { RenderPrepareStage::State });
    pImpl->abortSceneRenderPreparation();
    if (! pImpl->waitForPreparedUploads(pImpl->m_rendering_resources))
        return Err(RenderPrepareError { RenderPrepareStage::Upload });
    pImpl->clearLastRenderGraph(retention);
    return Ok(empty {});
}

auto VulkanRender::tryPrepareImage(Arc<vrento::Image> image)
    -> Result<bool, resource::ResourceError> {
    return pImpl->m_rendering_resources.resources.TryPrepareImage(rstd::move(image));
}

bool VulkanRender::hasSceneRenderState() const { return pImpl->m_scene_render_state.installed(); }

auto VulkanRender::tryInstallSceneRenderState(Scene& scene, vulkan::SceneRenderStateInput&& graph,
                                              const RenderSceneSnapshot& snapshot,
                                              SceneLoadBenchRecorderView bench)
    -> Result<empty, RenderPrepareError> {
    if (! pImpl->m_inited) return Err(RenderPrepareError { RenderPrepareStage::State });
    if (pImpl->m_frame) return Err(RenderPrepareError { RenderPrepareStage::Pending });
    if (pImpl->m_output) return Err(RenderPrepareError { RenderPrepareStage::State });
    pImpl->abortSceneRenderPreparation();
    if (! pImpl->waitForPreparedUploads(pImpl->m_rendering_resources))
        return Err(RenderPrepareError { RenderPrepareStage::Upload });
    return pImpl->installSceneRenderState(scene, rstd::move(graph), snapshot, bench);
}

auto VulkanRender::tryPrepareResources(Scene& scene, const RenderSceneSnapshot& snapshot,
                                       const ResourcePreparation& changes)
    -> Result<empty, RenderPrepareError> {
    auto ready = pImpl->preparationReady();
    if (ready.is_err()) return ready;
    if (! pImpl->m_scene_render_state.installed())
        return Err(RenderPrepareError { RenderPrepareStage::State });
    pImpl->configureRenderTargets(scene);
    if (changes.textures_only && pImpl->sceneRenderReady()) {
        auto current = ExtractRenderSceneSnapshot(scene);
        if (pImpl->m_scene_render_state.acceptsTextureContent(
                scene, current, scene.CaptureImageSource()))
            return pImpl->prepareSceneRenderState(
                scene, current, resource::ResourcePlanTextures, {});
    }
    if (! changes.textures_only && changes.texture_materials.is_empty() &&
        ! changes.items.is_empty() && pImpl->sceneRenderReady()) {
        bool content_only = true;
        for (const auto& item : changes.items) {
            content_only &=
                (item.flags & ~ToPassInvalidationFlags(PassInvalidation::Content)) == u32();
            content_only &= pImpl->m_scene_render_state.containsRenderItem(item.item);
        }
        if (content_only) return Ok(empty {});
    }
    return Err(RenderPrepareError { RenderPrepareStage::RebuildRequired });
}
void VulkanRender::configureRenderTargets(Scene& scene) { pImpl->configureRenderTargets(scene); }
void VulkanRender::installSceneRenderState(Scene& scene, vulkan::SceneRenderStateInput&& rg) {
    auto snapshot = ExtractRenderSceneSnapshot(scene);
    (void)tryInstallSceneRenderState(scene, rstd::move(rg), snapshot);
}
void VulkanRender::installSceneRenderState(Scene& scene, vulkan::SceneRenderStateInput&& rg,
                                           const RenderSceneSnapshot& snapshot) {
    (void)tryInstallSceneRenderState(scene, rstd::move(rg), snapshot);
}
void VulkanRender::installSceneRenderState(Scene& scene, vulkan::SceneRenderStateInput&& rg,
                                           const RenderSceneSnapshot& snapshot,
                                           SceneLoadBenchRecorderView bench) {
    (void)tryInstallSceneRenderState(scene, rstd::move(rg), snapshot, bench);
}
void VulkanRender::invalidatePreparedRenderItems(slice<owe::RenderItemId> items,
                                                 PassInvalidationFlags    flags) {
    if (pImpl->preparationReady().is_err()) return;
    pImpl->invalidatePreparedRenderItems(items, flags);
}
auto VulkanRender::diagnostics() const -> RenderDiagnostics {
    RenderDiagnostics value;
    if (pImpl->m_renderer) {
        value.compilation = pImpl->m_renderer->status();
        value.renderer    = pImpl->m_renderer->statistics();
        value.pipelines   = pImpl->m_renderer->session().graphics_resources().pipelines.stats();
    }
    value.preparation = pImpl->m_rendering_resources.resources.PrepareStatus().clone();
    value.resources   = pImpl->m_rendering_resources.resources.Statistics();
    value.frame_error = pImpl->m_last_frame_error;
    if (pImpl->m_frame) {
        value.frame       = Some(pImpl->m_frame->state());
        value.frame_error = pImpl->m_frame->error();
    }
    return value;
}
Vec<PreparedPassDiagnostic> VulkanRender::preparedPassDiagnostics() const {
    return pImpl->preparedPassDiagnostics();
}
void VulkanRender::evictUnusedMeshes() {
    if (pImpl->m_inited) pImpl->m_rendering_resources.resources.EvictUnusedBuffers();
};
void VulkanRender::UpdateCameraFillMode(Scene& scene, owe::FillMode fill) {
    pImpl->UpdateCameraFillMode(scene, fill);
};

bool VulkanRender::onSwapchainReady(unsigned width, unsigned height) {
    return pImpl->onSwapchainReady(width, height);
}

owe::ExSwapchain* VulkanRender::exSwapchain() const {
    return pImpl->m_ex_swapchain ? &(*pImpl->m_ex_swapchain)->AsSwapchain() : nullptr;
};

bool VulkanRender::Impl::init(RenderInitInfo info, SceneLoadBenchRecorderView load_bench) {
    if (m_inited) return true;
    if (m_session) destroy();

    m_redraw_cb = rstd::move(info.redraw_callback);
    VkExtent2D extent { info.width, info.height };
    m_extent = extent;
    if (extent.width * extent.height < 500 * 500) {
        rstd_error("too small swapchain image size: {}x{}", extent.width, extent.height);
    } else {
        rstd_info("set swapchain image size: {}x{}", extent.width, extent.height);
    }

    Vec<Extension> inst_exts;
    Vec<Extension> device_exts;
    for (const auto& extension : base_inst_exts) inst_exts.emplace_back(extension);
    for (const auto& extension : base_device_exts) device_exts.emplace_back(extension);
    if (info.video_hwdec.as_str() != "none"_str) {
        AppendVideoDeviceExtensions(device_exts);
    }

    if (! info.offscreen) {
        for (const auto& name : info.surface_info.instanceExts) {
            inst_exts.push(Extension { true, name.as_str() });
        }
        device_exts.push(
            { true, CStr::from_ptr(VK_KHR_SWAPCHAIN_EXTENSION_NAME).to_str().unwrap() });
    } else {
        // Iteration 1a: offscreen FDs are real Linux DMA-BUFs so they can be
        // imported by arbitrary external consumers. These extensions are
        // strictly required on the offscreen path; if a driver lacks them
        // we fail fast in Device::CheckGPU.
        device_exts.push(
            { true,
              CStr::from_ptr(VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME).to_str().unwrap() });
        // Required by VK_EXT_image_drm_format_modifier on Vulkan 1.1 (promoted
        // to core in 1.2). Validation layer rejects the device otherwise.
        device_exts.push(
            { true, CStr::from_ptr(VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME).to_str().unwrap() });
        device_exts.push(
            { true,
              CStr::from_ptr(VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME).to_str().unwrap() });
        device_exts.push(
            { true, CStr::from_ptr(VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME).to_str().unwrap() });
    }

    const auto instance_api_version =
        info.video_hwdec.as_str() == "none"_str ? WP_VULKAN_VERSION : VK_API_VERSION_1_3;
    {
        auto instance_span = SceneLoadSpan(load_bench, &SceneLoadProbeIds::vulkan_instance);
        Option<ViewerSurfaceFactory> surface_factory;
        if (! info.offscreen) {
            if (! info.surface_info.createSurfaceOp) {
                rstd_error("missing vulkan surface callback");
                return false;
            }
            surface_factory = Some(ViewerSurfaceFactory {
                .create = rstd::move(info.surface_info.createSurfaceOp).unwrap(),
            });
        }
        Option<mut_ref<dyn<vrento::vulkan::SessionSurfaceFactory>>> factory;
        if (surface_factory.is_some())
            factory = Some(dyn<vrento::vulkan::SessionSurfaceFactory>::from_ref(*surface_factory)
                               .as_mut_ref());
        auto created = vrento::vulkan::DeviceSession::create(
            {
                .instance_extensions  = inst_exts.as_slice(),
                .device_extensions    = device_exts.as_slice(),
                .physical_device_uuid = info.uuid ? info.uuid->as_slice() : slice<rstd::uint8_t> {},
                .api_version          = instance_api_version,
                .extent               = extent,
                .validation           = info.enable_valid_layer,
            },
            factory);
        if (created.is_err()) {
            const auto error = created.unwrap_err();
            rstd_error("init vulkan session failed: stage={}, result={}",
                       static_cast<int>(error.stage),
                       static_cast<int>(error.result));
            return false;
        }
        m_session  = Some(rstd::move(created).unwrap());
        m_renderer = Some(vrento::vulkan::Renderer(*m_session));
        vrento::vulkan::register_texture_factories(*m_renderer).unwrap();
        vrento::vulkan::register_graphics_factories(*m_renderer).unwrap();
        vrento::vulkan::register_surface_factories(*m_renderer).unwrap();
        m_device       = rstd::addressof(m_session->device());
        m_with_surface = m_session->has_surface();
    }

    {
        auto device_span = SceneLoadSpan(load_bench, &SceneLoadProbeIds::vulkan_device);
        m_extent         = m_device->out_extent();
        if (! m_rendering_resources.resources.Initialize(*m_session)) {
            rstd_error("resource registry init failed");
            return false;
        }
        if (info.pipeline_preparation == vrento::vulkan::PipelinePreparationMode::Deferred &&
            m_rendering_resources.resources.SetPipelinePreparationMode(info.pipeline_preparation)
                .is_err())
            return false;
#if __is_target_os(macos)
        if (! info.offscreen) {
            // Surface output uses a fullscreen graphics pass. The
            // swapchain format is fixed when Device::Create returns, so it
            // can be part of the render-pass/pipeline resource plan.
            m_present_format = m_device->swapchain().format();
        }
#endif
        m_rendering_resources.resources.SetVideoDecodeOptions({
            .hwdec       = rstd::move(info.video_hwdec),
            .render_node = rstd::move(info.video_render_node),
        });
    }

    {
        // Map requested integer to a bit; clamp down to highest supported bit
        // not exceeding the request, given device's framebufferColorSampleCounts.
        const rstd::uint32_t     requested = info.msaa_samples == 0 ? 1u : info.msaa_samples;
        const VkSampleCountFlags supported = m_device->limits().framebufferColorSampleCounts;
        VkSampleCountFlagBits    chosen    = VK_SAMPLE_COUNT_1_BIT;
        constexpr rstd::array<VkSampleCountFlagBits, 6> ladder {
            VK_SAMPLE_COUNT_64_BIT, VK_SAMPLE_COUNT_32_BIT, VK_SAMPLE_COUNT_16_BIT,
            VK_SAMPLE_COUNT_8_BIT,  VK_SAMPLE_COUNT_4_BIT,  VK_SAMPLE_COUNT_2_BIT,
        };
        for (auto bit : ladder) {
            if (static_cast<rstd::uint32_t>(bit) <= requested && (supported & bit)) {
                chosen = bit;
                break;
            }
        }
        m_msaa_samples = chosen;
        rstd_info(
            "msaa requested={} actual={}", requested, static_cast<rstd::uint32_t>(m_msaa_samples));
    }

    if (info.offscreen) {
        auto swapchain_span = SceneLoadSpan(load_bench, &SceneLoadProbeIds::vulkan_swapchain);
        if (info.ex_swapchain_factory) {
            RenderInitInfo::ExSwapchainHandles h {
                m_session->instance(),
                *m_device->gpu(),
                *m_device->handle(),
                *m_device->graphics_queue().handle,
                m_device->graphics_queue().family_index,
                m_device->instance_dispatch().resolver,
            };
            m_ex_swapchain = (*info.ex_swapchain_factory)->call_once(h);
            if (! m_ex_swapchain) {
                rstd_error("ex_swapchain_factory returned null");
                return false;
            }
        } else {
            m_ex_swapchain = m_rendering_resources.resources.CreateLocalSwapchain(
                *m_device,
                extent.width,
                extent.height,
                (info.offscreen_tiling == TexTiling::OPTIMAL ? VK_IMAGE_TILING_OPTIMAL
                                                             : VK_IMAGE_TILING_LINEAR));
        }
        m_with_surface = false;
    }

    {
        auto resources_span = SceneLoadSpan(load_bench, &SceneLoadProbeIds::vulkan_resources);
        if (! initRes()) return false;
    }

    m_inited = true;
    return m_inited;
}

bool VulkanRender::Impl::initRes() {
    if (! CreateRenderingResource(m_rendering_resources)) return false;

    return true;
}

void VulkanRender::Impl::destroy() {
    m_last_frame_error = None();
    if (! m_session && ! m_inited) return;
    if (m_device && m_device->handle()) {
        closeOutput();
        if (m_renderer) m_renderer->clear();
        m_rendering_resources.resources.AbortPreparePlan();

        // res
        m_scene_render_state.clearBindings();
        m_texture_output.clear();
        m_graphics = None();
        ReleaseCompletedRetiredResources(*m_device, m_rendering_resources);
        m_scene_render_state.clear();
        m_rendering_resources.resources.Reset();
    }
    m_sem_swap_finish_per_image.clear();
    DestroyRenderingResource(m_rendering_resources);
    m_ex_swapchain   = None();
    m_device         = nullptr;
    m_renderer       = None();
    m_session        = None();
    m_inited         = false;
    m_present_format = VK_FORMAT_UNDEFINED;
}

bool VulkanRender::Impl::CreateRenderingResource(RenderingResources& rr) {
    if (m_with_surface) {
        VkSemaphoreCreateInfo info { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        m_sem_swap_finish_per_image.clear();
        for (usize i {}; i < m_device->swapchain().images().len(); ++i) {
            auto semaphore = Arc<vvk::Semaphore>::make();
            VVK_CHECK_BOOL_RE(m_device->handle().CreateSemaphore(info, *semaphore));
            m_sem_swap_finish_per_image.push(rstd::move(semaphore));
        }
    }
    rr.shader_reflection_cache =
        Some(mut_ref<ShaderReflectionCache>::from_raw_parts(&m_shader_reflection_cache));
    return true;
}
void VulkanRender::Impl::DestroyRenderingResource(RenderingResources& rr) {
    rr.scopes                  = Arc<vrento::vulkan::GraphicsScopeCache>::make();
    rr.shader_reflection_cache = None();
}

bool VulkanRender::Impl::waitForPreparedUploads(RenderingResources& rr) {
    auto wait_span =
        SceneLoadSpan(m_pending_load_bench, &SceneLoadProbeIds::render_texture_upload_wait);
    if (! rr.resources.WaitForUploads(vk_wait_time)) return false;
    m_pending_load_bench = {};
    return true;
}

void VulkanRender::Impl::drawFrame(Scene& scene) {
    m_frame_submitted = false;
    if (m_output) {
        if (m_with_surface)
            drawFrameSwapchain(scene);
        else
            drawFrameOffscreen(scene);
        if (m_redraw_cb) (*m_redraw_cb)->operator()();
        return;
    }
    if (m_inited && m_renderer->pending()) {
        auto status  = continueSceneRenderPreparation(scene);
        auto resumed = finishSceneRenderPreparation(scene, status);
        if (resumed.is_err()) return;
    }
    if (! (m_inited && sceneRenderReady())) return;

    if (! m_with_surface) {
        drawFrameOffscreen(scene);
    } else {
        drawFrameSwapchain(scene);
    }

    if (m_redraw_cb) (*m_redraw_cb)->operator()();
}

void VulkanRender::Impl::drawFrameSwapchain(Scene& scene) {
    auto& rr = m_rendering_resources;
    if (! m_output) {
        if (! waitForPreparedUploads(rr)) return;
        auto textures = dyn<SceneTextureAnimationView>::from_ref(scene);
        if (! m_scene_render_state.update(scene.Runtime().Frame(), m_extent, textures.as_ref(), rr))
            return;
        auto synchronization = createFrameSynchronization(false);
        if (! synchronization) return;
        auto           owner = synchronization.take().unwrap();
        rstd::uint32_t image_index {};
        auto           acquired = m_device->handle().AcquireNextImageKHR(
            *m_device->swapchain().handle(), vk_wait_time, **owner->acquire, {}, &image_index);
        if (acquired != VK_SUCCESS && acquired != VK_SUBOPTIMAL_KHR) return;
        owner->signal     = m_sem_swap_finish_per_image[usize(image_index)].clone();
        const auto serial = m_next_surface_acquire_serial++;
        if (serial == u64()) rstd::panic("window acquisition serial exhausted");
        FrameSurfaceLease lease {
            .identity             = { u64(1), u32(image_index), serial },
            .reuse                = { .kind = FrameSurfaceReuseKind::PresentationAcquired },
            .image                = m_device->swapchain().images()[usize(image_index)],
            .format               = m_device->swapchain().format(),
            .initial_layout       = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .initial_queue_family = m_device->graphics_queue().family_index,
            .acquire              = { .kind      = FrameSurfaceAcquireKind::BinarySemaphore,
                                      .semaphore = **owner->acquire },
            .final_layout         = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .final_queue_family   = m_device->graphics_queue().family_index,
            .discard_content      = true,
        };
        auto target = vrento::vulkan::SurfaceTarget::create(
            *m_session, lease, Arc<dyn<rstd::any::Any>>::make(owner.clone()));
        if (target.is_err()) rstd::panic("invalid acquired window target");
        m_output =
            Some(HostOutputFrame { target.unwrap(), rstd::move(owner), None(), Some(image_index) });
        auto started = rr.resources.BeginFrame();
        if (started.is_err() || ! beginFrame(started.unwrap())) {
            failOutput();
            return;
        }
    }
    if (! executeFrame()) return;
    auto             index = *m_output->window_image;
    VkPresentInfoKHR info { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores    = m_output->synchronization->signal->address();
    info.swapchainCount     = 1;
    info.pSwapchains        = m_device->swapchain().handle().address();
    info.pImageIndices      = &index;
    auto result             = m_device->present_queue().handle.Present(info);
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        m_output->blocked = true;
        rstd_error("window presentation failed: {}", static_cast<int>(result));
        return;
    }
    ReleaseCompletedRetiredResources(*m_device, rr);
    m_frame  = None();
    m_output = None();
}
void VulkanRender::Impl::drawFrameOffscreen(Scene& scene) {
    if (! m_ex_swapchain) return;
    auto& rr = m_rendering_resources;
    if (! m_output) {
        (*m_ex_swapchain)->AsSwapchain().poll();
        if (! (*m_ex_swapchain)->AsSwapchain().ready() || ! sceneRenderReady()) return;
        if (! waitForPreparedUploads(rr)) return;
        auto textures = dyn<SceneTextureAnimationView>::from_ref(scene);
        if (! m_scene_render_state.update(scene.Runtime().Frame(), m_extent, textures.as_ref(), rr))
            return;
        auto synchronization = createFrameSynchronization(true);
        if (! synchronization) return;
        auto acquired = (*m_ex_swapchain)->AsSwapchain().acquireRenderTarget();
        if (! acquired.acquired()) return;
        auto owner  = synchronization.take().unwrap();
        auto target = vrento::vulkan::SurfaceTarget::create(
            *m_session, acquired.lease, Arc<dyn<rstd::any::Any>>::make(owner.clone()));
        if (target.is_err()) return;
        m_output     = Some(HostOutputFrame {
            target.unwrap(), rstd::move(owner), Some(rstd::move(acquired.completion)), None() });
        auto started = rr.resources.BeginFrame();
        if (started.is_err() || ! beginFrame(started.unwrap())) {
            failOutput();
            return;
        }
    }
    if (! executeFrame()) return;
    int sync_fd = -1;
    if (m_device->handle().Dispatch().vkGetSemaphoreFdKHR) {
        VkSemaphoreGetFdInfoKHR info { VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR };
        info.semaphore  = **m_output->synchronization->signal;
        info.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT_KHR;
        auto result     = m_device->handle().GetSemaphoreFdKHR(info, &sync_fd);
        if (result != VK_SUCCESS) {
            m_output->blocked = true;
            rstd_error("frame synchronization export failed: {}", static_cast<int>(result));
            return;
        }
    }
    auto completed = m_output->completion->Submit(sync_fd);
    if (completed.status != FrameSurfaceCompletionStatus::Submitted &&
        completed.status != FrameSurfaceCompletionStatus::SessionLost) {
        m_output->blocked = true;
        rstd_error("output completion failed: {}", static_cast<int>(completed.status));
        return;
    }
    ReleaseCompletedRetiredResources(*m_device, rr);
    m_frame  = None();
    m_output = None();
}

bool VulkanRender::Impl::onSwapchainReady(unsigned width, unsigned height) {
    if (! m_inited) return false;
    auto& cur            = m_extent;
    bool  extent_changed = (width != cur.width) || (height != cur.height);
    if (! extent_changed) {
        // Format-only changes flow through ExSwapchain::format() and
        // are handled by drawFrameOffscreen's head check.
        return false;
    }
    if (! m_output && ! waitForPreparedUploads(m_rendering_resources)) return false;
    m_extent = VkExtent2D { width, height };
    return true;
}

void VulkanRender::Impl::UpdateCameraFillMode(owe::Scene& scene, owe::FillMode fillmode) {
    auto width  = m_extent.width;
    auto height = m_extent.height;

    if (width == 0) return;
    auto   projection_extent = scene.OrthographicProjectionExtent();
    double sw = projection_extent[usize()], sh = projection_extent[usize(1)];
    double fboAspect = width / (double)height, sAspect = sw / sh;
    auto   global      = scene.CameraMut("global"_str);
    auto   perspective = scene.CameraMut("global_perspective"_str);
    if (global.is_none() || perspective.is_none()) return;
    auto& gCam    = **global;
    auto& gPerCam = **perspective;
    // assum cam
    switch (fillmode) {
    case FillMode::STRETCH:
        gCam.SetWidth(sw);
        gCam.SetHeight(sh);
        gPerCam.SetAspect(sAspect);
        if (! gPerCam.IsLookAt())
            gPerCam.SetFov(algorism::CalculatePersperctiveFov(1000.0f, gCam.Height()));
        break;
    case FillMode::ASPECTFIT:
        if (fboAspect < sAspect) {
            // scale height
            gCam.SetWidth(sw);
            gCam.SetHeight(sw / fboAspect);
        } else {
            gCam.SetWidth(sh * fboAspect);
            gCam.SetHeight(sh);
        }
        gPerCam.SetAspect(fboAspect);
        if (! gPerCam.IsLookAt())
            gPerCam.SetFov(algorism::CalculatePersperctiveFov(1000.0f, gCam.Height()));
        break;
    case FillMode::ASPECTCROP:
    default:
        if (fboAspect > sAspect) {
            // scale height
            gCam.SetWidth(sw);
            gCam.SetHeight(sw / fboAspect);
        } else {
            gCam.SetWidth(sh * fboAspect);
            gCam.SetHeight(sh);
        }
        gPerCam.SetAspect(fboAspect);
        if (! gPerCam.IsLookAt())
            gPerCam.SetFov(algorism::CalculatePersperctiveFov(1000.0f, gCam.Height()));
        break;
    }
    gCam.Update();
    gPerCam.Update();
    scene.UpdateLinkedCamera("global"_str);
    scene.CaptureCameraPathViewports();
}

void VulkanRender::Impl::clearLastRenderGraph(RenderGraphResourceRetention retention) {
    if (m_renderer) m_renderer->clear();
    m_rendering_resources.resources.AbortPreparePlan();
    m_scene_render_state.clearBindings();
    m_texture_output.clear();
    m_graphics = None();
    ReleaseCompletedRetiredResources(*m_device, m_rendering_resources);
    m_scene_render_state.clear();
    m_rendering_resources.resources.ClearPreparedGraphics();
    if (retention == RenderGraphResourceRetention::ReleaseSceneTextures) {
        m_rendering_resources.resources.ClearTextures();
        m_shader_reflection_cache.Clear();
    } else {
        m_rendering_resources.resources.ClearTransientTextures();
    }
    m_rendering_resources.resources.EvictUnusedBuffers();
}

void VulkanRender::Impl::configureRenderTargets(Scene& scene) {
    if (! m_inited) return;
    const auto&      limits = m_device->limits();
    const VkExtent2D max_framebuffer_extent {
        rstd::cmp::min(limits.maxFramebufferWidth, limits.maxImageDimension2D),
        rstd::cmp::min(limits.maxFramebufferHeight, limits.maxImageDimension2D),
    };
    FinalizeRenderTargetSizes(scene, m_extent, max_framebuffer_extent, m_msaa_samples);
}

auto VulkanRender::Impl::prepareSceneRenderState(Scene&                         scene,
                                                 const RenderSceneSnapshot&     render_scene,
                                                 resource::ResourcePlanSections sections,
                                                 SceneLoadBenchRecorderView     load_bench)
    -> Result<empty, RenderPrepareError> {
    // Prepared refresh replaces binding owners before recreating equivalent bindings.
    [[maybe_unused]] auto audio_demand_reconciliation =
        scene.AudioDemandMut()->BeginReconciliation();
    if (sections == resource::ResourcePlanTextures && ! m_graphics)
        return Err(RenderPrepareError { RenderPrepareStage::State });
    if (resource::ResourcePlanIncludes(sections, resource::ResourcePlanBuffers) !=
        resource::ResourcePlanIncludes(sections, resource::ResourcePlanShaders))
        return Err(RenderPrepareError { RenderPrepareStage::State });
    auto declarations = m_scene_render_state.declarations(m_shader_reflection_cache);
    SnapshotImportedTextureProvider imported(render_scene, m_scene_render_state.imageSource(scene));
    SnapshotTexturePrepareObserver  observer(load_bench);
    auto content  = dyn<resource::TextureContentProvider>::from_ref(imported);
    auto observed = dyn<resource::TexturePrepareObserver>::from_ref(observer);
    auto buffers  = dyn<resource::BufferContentProvider>::from_ref(declarations);
    DeclaredShaderArtifactProvider shaders(declarations);
    auto artifacts = dyn<resource::ShaderArtifactProvider>::from_ref(shaders);
    auto started   = m_rendering_resources.resources.BeginPreparePlan(
        m_scene_render_state.resourcePlan(),
        resource_registry::ResourceContentProviders { .texture = Some(content),
                                                      .buffer  = Some(buffers.as_mut_ref()),
                                                      .shader  = Some(artifacts.as_mut_ref()) },
        sections,
        Some(observed.as_mut_ref()));
    m_scene_render_state.retainDeclarations(declarations.takeData());
    if (started.is_err()) {
        rstd_error("prepare resource plan failed: {}", started.unwrap_err().message);
        return finishSceneRenderPreparation(
            scene, SceneRenderPreparationProgress::Failed, load_bench);
    }
    return finishSceneRenderPreparation(
        scene, continueSceneRenderPreparation(scene, load_bench), load_bench);
}

bool VulkanRender::Impl::sceneRenderReady() const {
    if (! m_scene_render_state.valid() || ! m_graphics || ! m_renderer || ! m_renderer->ready() ||
        m_renderer->status().kind != vrento::vulkan::CompileKind::Graph ||
        m_rendering_resources.resources.PrepareStatus().state !=
            resource_registry::ResourcePrepareState::Committed)
        return false;
    auto compiled = m_renderer->compiled();
    return compiled && compiled->description().scope() == m_scene_render_state.inputGraph().scope();
}
void VulkanRender::Impl::abortSceneRenderPreparation() {
    m_renderer->cancel_compile();
    m_rendering_resources.resources.AbortPreparePlan();
}
auto VulkanRender::Impl::continueSceneRenderPreparation(Scene&                     scene,
                                                        SceneLoadBenchRecorderView bench)
    -> SceneRenderPreparationProgress {
    using Progress = SceneRenderPreparationProgress;
    if (m_renderer->pending()) {
        return SceneCompileProgress(m_renderer->continue_compile(m_renderer->status().id));
    }
    auto&       resources   = m_rendering_resources.resources;
    const auto& transaction = resources.PrepareStatus();
    if (transaction.state != resource_registry::ResourcePrepareState::Preparing)
        return Progress::Failed;
    SnapshotTexturePrepareObserver observer(bench);
    auto observed = dyn<resource::TexturePrepareObserver>::from_ref(observer);
    auto progress = resources.ContinuePreparePlan(transaction.id, Some(observed.as_mut_ref()));
    if (progress.is_err()) {
        rstd_error("prepare resource plan failed: {}", progress.unwrap_err().message);
        return Progress::Failed;
    }
    if (*progress == resource_registry::ResourcePrepareProgress::BatchReady)
        return Progress::BatchReady;
    if (transaction.sections == resource::ResourcePlanTextures) return Progress::Complete;
    {
        auto shared = m_scene_render_state.sharedBuffers(resources.Prepared());
        if (shared.is_err()) return Progress::Failed;
        Vec<vrento::vulkan::PipelineLayoutRequirement> additional;
        if (auto output = m_texture_output.layout_requirement()) additional.push(output.unwrap());
        auto prepared = vrento::vulkan::PreparedGraphicsResources::prepare(
            m_rendering_resources.resource_owner.clone(),
            m_scene_render_state.inputDescriptions(),
            shared->as_slice(),
            rstd::move(additional),
            { .shared_set = Some(u32()) });
        if (prepared.is_err()) {
            rstd_error("prepare graphics resources failed: {}", prepared.unwrap_err().message);
            return Progress::Failed;
        }
        m_graphics = Some(prepared.unwrap());
        for (const auto& conflict : (*m_graphics)->conflicts())
            rstd_debug("pipeline {} did not join layout family {}: {}",
                       conflict.pipeline.index,
                       conflict.family,
                       conflict.message);
    }
    if (! m_graphics ||
        ! m_scene_render_state.bindResources(scene, m_rendering_resources, **m_graphics))
        return Progress::Failed;
    if (m_texture_output
            .prepare(m_rendering_resources.resource_owner.clone(),
                     m_shader_backend.as_ref(),
                     (*m_graphics)->layouts())
            .is_err())
        return Progress::Failed;
    Vec<vrento::vulkan::PassInput> descriptions;
    for (const auto& desc : m_scene_render_state.inputDescriptions()) {
        if (m_scene_render_state.isOutputPass(desc.execution().id)) {
            auto published = m_texture_output.description(desc, desc.revision());
            if (published.is_err()) return Progress::Failed;
            descriptions.push(published.unwrap());
        } else
            descriptions.push(desc.clone());
    }
    auto input = m_scene_render_state.renderInput().with_descriptions(rstd::move(descriptions));
    if (input.is_err()) return Progress::Failed;
    return SceneCompileProgress(
        m_renderer->compile(input.unwrap(), Some((*m_graphics)->compilation())));
}

auto VulkanRender::Impl::finishSceneRenderPreparation(Scene&                         scene,
                                                      SceneRenderPreparationProgress status,
                                                      SceneLoadBenchRecorderView     load_bench)
    -> Result<empty, RenderPrepareError> {
    while (status == SceneRenderPreparationProgress::BatchReady) {
        status = continueSceneRenderPreparation(scene, load_bench);
    }
    if (status == SceneRenderPreparationProgress::Pending)
        return Err(RenderPrepareError { RenderPrepareStage::Pending });
    if (status == SceneRenderPreparationProgress::Failed) {
        abortSceneRenderPreparation();
        return Err(RenderPrepareError { RenderPrepareStage::SceneRenderState });
    }

    auto  upload_span = SceneLoadSpan(load_bench, &SceneLoadProbeIds::render_upload_submit);
    auto& resources   = m_rendering_resources.resources;
    auto  committed   = resources.CommitPreparePlan(resources.PrepareStatus().id);
    if (committed.is_err()) {
        abortSceneRenderPreparation();
        return Err(RenderPrepareError { RenderPrepareStage::Upload });
    }
    return Ok(empty {});
}

auto VulkanRender::Impl::installSceneRenderState(Scene& scene, vulkan::SceneRenderStateInput&& rg,
                                                 const RenderSceneSnapshot& render_scene,
                                                 SceneLoadBenchRecorderView load_bench)
    -> Result<empty, RenderPrepareError> {
    if (! m_inited) return Err(RenderPrepareError { RenderPrepareStage::State });
    m_pending_load_bench = load_bench;
    auto compile_span    = SceneLoadSpan(load_bench, &SceneLoadProbeIds::render_graph_compile);
    m_renderer->clear();

    {
        auto state_span = SceneLoadSpan(load_bench, &SceneLoadProbeIds::render_program_build);
        SceneRenderState next;
        auto             installed = next.install(rstd::move(rg));
        if (installed.is_err()) {
            auto error = installed.unwrap_err();
            rstd_error("scene render state build failed: kind={}, pass={}, resource={}",
                       unsigned(error.kind),
                       error.pass.value,
                       error.resource.value);
            return Err(RenderPrepareError { RenderPrepareStage::Graph, Some(error) });
        }
        abortSceneRenderPreparation();
        m_scene_render_state.clearBindings();
        m_texture_output.clear();
        m_graphics = None();
        m_rendering_resources.resources.ClearPreparedGraphics();
        m_scene_render_state = rstd::move(next);
        m_texture_output =
            m_scene_render_state.createOutput(m_present_format, m_shader_reflection_cache);
    }

    return prepareSceneRenderState(scene, render_scene, resource::ResourcePlanAll, load_bench);
};

void VulkanRender::Impl::invalidatePreparedRenderItems(slice<owe::RenderItemId> render_items,
                                                       PassInvalidationFlags    flags) {
    if (! m_inited) return;
    m_scene_render_state.invalidateRenderItems(render_items, flags);
}

Vec<PreparedPassDiagnostic> VulkanRender::Impl::preparedPassDiagnostics() const {
    auto compiled = m_renderer ? m_renderer->compiled() : None<vrento::vulkan::CompiledGraph>();
    return m_scene_render_state.diagnostics(sceneRenderReady(),
                                            m_graphics ? (*m_graphics).as_ptr() : nullptr,
                                            compiled ? compiled->descriptions()
                                                     : slice<vrento::vulkan::PassInput>());
}

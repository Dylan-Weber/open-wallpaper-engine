module;
#include <unistd.h>

export module wescene.capture_surface;

import rstd;
import vvk;
import wescene.vulkan;
import wescene.vulkan_render;

using namespace rstd::prelude;
using namespace rstd::literals;

export namespace owe
{

// The frame an offscreen renderer should keep, and its size.
struct CaptureRequest {
    u32      frame { 0 };
    unsigned width { 1280 };
    unsigned height { 720 };
};

// Gets the frame as tightly packed RGBA rows, top to bottom, or None when it couldn't be read.
using CaptureCallback = Box<dyn<FnMut<void(Option<Vec<u8>>)>>>;

// A frame surface for offscreen renderers that keeps one frame: every frame is drawn into one
// image, and frame N (counted from 0, the first frame drawn) is copied to the host. The scene's
// clock advances one frame time (1 / fps) a frame, so frame N shows the scene at N / fps
// seconds, however long loading or drawing took. After it, the surface draws no more frames.
class CaptureSurface : public ExSwapchain {
public:
    CaptureSurface(const RenderInitInfo::ExSwapchainHandles& handles, CaptureRequest request,
                   CaptureCallback on_captured);
    ~CaptureSurface() override;

    unsigned width() const override { return m_request.width; }
    unsigned height() const override { return m_request.height; }
    VkFormat format() const override { return VK_FORMAT_R8G8B8A8_UNORM; }
    bool     ready() const override { return m_view != VK_NULL_HANDLE; }
    void     setOnReadyChanged(Option<ExSwapchainReadyCallback>) override {}
    auto     acquireRenderTarget() -> FrameSurfaceAcquireResult override;

private:
    auto CompleteRendered(FrameSurfaceIdentity identity, int producer_sync_fd)
        -> FrameSurfaceCompletionResult override;
    auto AbortRenderTarget(FrameSurfaceIdentity identity) -> FrameSurfaceCompletionResult override;

    struct Functions {
        PFN_vkGetPhysicalDeviceMemoryProperties memory_properties;
        PFN_vkCreateImage                       create_image;
        PFN_vkDestroyImage                      destroy_image;
        PFN_vkCreateImageView                   create_view;
        PFN_vkDestroyImageView                  destroy_view;
        PFN_vkGetImageMemoryRequirements        image_requirements;
        PFN_vkBindImageMemory                   bind_image;
        PFN_vkCreateBuffer                      create_buffer;
        PFN_vkDestroyBuffer                     destroy_buffer;
        PFN_vkGetBufferMemoryRequirements       buffer_requirements;
        PFN_vkBindBufferMemory                  bind_buffer;
        PFN_vkAllocateMemory                    allocate;
        PFN_vkFreeMemory                        free;
        PFN_vkMapMemory                         map;
        PFN_vkUnmapMemory                       unmap;
        PFN_vkCreateCommandPool                 create_pool;
        PFN_vkDestroyCommandPool                destroy_pool;
        PFN_vkAllocateCommandBuffers            allocate_commands;
        PFN_vkBeginCommandBuffer                begin;
        PFN_vkEndCommandBuffer                  end;
        PFN_vkCmdPipelineBarrier                barrier;
        PFN_vkCmdCopyImageToBuffer              copy;
        PFN_vkQueueSubmit                       submit;
        PFN_vkQueueWaitIdle                     wait_idle;
    };

    bool load(const RenderInitInfo::ExSwapchainHandles& handles);
    bool create(VkPhysicalDevice physical_device);
    auto allocate(VkMemoryRequirements needs, VkMemoryPropertyFlags flags) -> VkDeviceMemory;
    auto read() -> Option<Vec<u8>>;

    CaptureRequest                   m_request;
    CaptureCallback                  m_on_captured;
    bool                             m_captured { false };
    Functions                        m_vk {};
    VkDevice                         m_device { VK_NULL_HANDLE };
    VkQueue                          m_queue { VK_NULL_HANDLE };
    rstd::uint32_t                   m_queue_family { 0 };
    VkPhysicalDeviceMemoryProperties m_memory_types {};
    VkImage                          m_image { VK_NULL_HANDLE };
    VkDeviceMemory                   m_image_memory { VK_NULL_HANDLE };
    VkImageView                      m_view { VK_NULL_HANDLE };
    VkBuffer                         m_buffer { VK_NULL_HANDLE };
    VkDeviceMemory                   m_buffer_memory { VK_NULL_HANDLE };
    VkCommandPool                    m_pool { VK_NULL_HANDLE };
    VkCommandBuffer                  m_commands { VK_NULL_HANDLE };
    u64                              m_serial { 0 };
    u32                              m_frames { 0 };
    bool                             m_defined { false };
};

// For RenderInitInfo::ex_swapchain_factory.
auto MakeCaptureSurfaceFactory(CaptureRequest request, CaptureCallback on_captured)
    -> Box<dyn<FnOnce<Option<ExSwapchainOwner>(const RenderInitInfo::ExSwapchainHandles&)>>>;

} // namespace owe

namespace owe
{

CaptureSurface::CaptureSurface(const RenderInitInfo::ExSwapchainHandles& handles,
                               CaptureRequest request, CaptureCallback on_captured)
    : m_request(rstd::move(request)), m_on_captured(rstd::move(on_captured)) {
    if (! load(handles) || ! create(handles.physical_device)) {
        m_captured = true;
        m_on_captured->operator()(None());
    }
}

CaptureSurface::~CaptureSurface() {
    if (m_device == VK_NULL_HANDLE) return;
    if (m_pool) m_vk.destroy_pool(m_device, m_pool, nullptr);
    if (m_buffer) m_vk.destroy_buffer(m_device, m_buffer, nullptr);
    if (m_buffer_memory) m_vk.free(m_device, m_buffer_memory, nullptr);
    if (m_view) m_vk.destroy_view(m_device, m_view, nullptr);
    if (m_image) m_vk.destroy_image(m_device, m_image, nullptr);
    if (m_image_memory) m_vk.free(m_device, m_image_memory, nullptr);
}

bool CaptureSurface::load(const RenderInitInfo::ExSwapchainHandles& handles) {
    auto instance_proc = handles.get_instance_proc_addr;
    auto device_proc   = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
        instance_proc(handles.instance, "vkGetDeviceProcAddr"));
    if (device_proc == nullptr) return false;
    bool loaded = true;
    auto device = [&]<typename F>(F& out, const char* name) {
        out    = reinterpret_cast<F>(device_proc(handles.device, name));
        loaded = loaded && out != nullptr;
    };
    m_vk.memory_properties = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(
        instance_proc(handles.instance, "vkGetPhysicalDeviceMemoryProperties"));
    loaded = m_vk.memory_properties != nullptr;
    device(m_vk.create_image, "vkCreateImage");
    device(m_vk.destroy_image, "vkDestroyImage");
    device(m_vk.create_view, "vkCreateImageView");
    device(m_vk.destroy_view, "vkDestroyImageView");
    device(m_vk.image_requirements, "vkGetImageMemoryRequirements");
    device(m_vk.bind_image, "vkBindImageMemory");
    device(m_vk.create_buffer, "vkCreateBuffer");
    device(m_vk.destroy_buffer, "vkDestroyBuffer");
    device(m_vk.buffer_requirements, "vkGetBufferMemoryRequirements");
    device(m_vk.bind_buffer, "vkBindBufferMemory");
    device(m_vk.allocate, "vkAllocateMemory");
    device(m_vk.free, "vkFreeMemory");
    device(m_vk.map, "vkMapMemory");
    device(m_vk.unmap, "vkUnmapMemory");
    device(m_vk.create_pool, "vkCreateCommandPool");
    device(m_vk.destroy_pool, "vkDestroyCommandPool");
    device(m_vk.allocate_commands, "vkAllocateCommandBuffers");
    device(m_vk.begin, "vkBeginCommandBuffer");
    device(m_vk.end, "vkEndCommandBuffer");
    device(m_vk.barrier, "vkCmdPipelineBarrier");
    device(m_vk.copy, "vkCmdCopyImageToBuffer");
    device(m_vk.submit, "vkQueueSubmit");
    device(m_vk.wait_idle, "vkQueueWaitIdle");
    if (! loaded) return false;
    m_device       = handles.device;
    m_queue        = handles.graphics_queue;
    m_queue_family = handles.graphics_queue_family;
    m_vk.memory_properties(handles.physical_device, &m_memory_types);
    return true;
}

auto CaptureSurface::allocate(VkMemoryRequirements needs, VkMemoryPropertyFlags flags)
    -> VkDeviceMemory {
    for (rstd::uint32_t type = 0; type < m_memory_types.memoryTypeCount; ++type) {
        if (! (needs.memoryTypeBits & (1u << type)) ||
            (m_memory_types.memoryTypes[type].propertyFlags & flags) != flags)
            continue;
        VkMemoryAllocateInfo info { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        info.allocationSize  = needs.size;
        info.memoryTypeIndex = type;
        VkDeviceMemory memory { VK_NULL_HANDLE };
        if (m_vk.allocate(m_device, &info, nullptr, &memory) == VK_SUCCESS) return memory;
    }
    return VK_NULL_HANDLE;
}

bool CaptureSurface::create(VkPhysicalDevice) {
    VkImageCreateInfo image { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    image.imageType   = VK_IMAGE_TYPE_2D;
    image.format      = format();
    image.extent      = { m_request.width, m_request.height, 1 };
    image.mipLevels   = 1;
    image.arrayLayers = 1;
    image.samples     = VK_SAMPLE_COUNT_1_BIT;
    image.tiling      = VK_IMAGE_TILING_OPTIMAL;
    image.usage       = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (m_vk.create_image(m_device, &image, nullptr, &m_image) != VK_SUCCESS) return false;
    VkMemoryRequirements image_needs {};
    m_vk.image_requirements(m_device, m_image, &image_needs);
    m_image_memory = allocate(image_needs, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (! m_image_memory || m_vk.bind_image(m_device, m_image, m_image_memory, 0) != VK_SUCCESS)
        return false;

    VkBufferCreateInfo buffer { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    buffer.size  = VkDeviceSize(m_request.width) * m_request.height * 4;
    buffer.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (m_vk.create_buffer(m_device, &buffer, nullptr, &m_buffer) != VK_SUCCESS) return false;
    VkMemoryRequirements buffer_needs {};
    m_vk.buffer_requirements(m_device, m_buffer, &buffer_needs);
    m_buffer_memory = allocate(
        buffer_needs, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (! m_buffer_memory || m_vk.bind_buffer(m_device, m_buffer, m_buffer_memory, 0) != VK_SUCCESS)
        return false;

    VkCommandPoolCreateInfo pool { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    pool.queueFamilyIndex = m_queue_family;
    if (m_vk.create_pool(m_device, &pool, nullptr, &m_pool) != VK_SUCCESS) return false;
    VkCommandBufferAllocateInfo commands { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    commands.commandPool        = m_pool;
    commands.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commands.commandBufferCount = 1;
    if (m_vk.allocate_commands(m_device, &commands, &m_commands) != VK_SUCCESS) return false;

    VkImageViewCreateInfo view { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    view.image            = m_image;
    view.viewType         = VK_IMAGE_VIEW_TYPE_2D;
    view.format           = format();
    view.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    return m_vk.create_view(m_device, &view, nullptr, &m_view) == VK_SUCCESS;
}

auto CaptureSurface::acquireRenderTarget() -> FrameSurfaceAcquireResult {
    if (! ready() || m_captured) return { .status = FrameSurfaceAcquireStatus::NotReady };
    FrameSurfaceIdentity identity { u64(1), u32(), ++m_serial };
    FrameSurfaceLease    lease;
    lease.identity             = identity;
    lease.image.handle         = m_image;
    lease.image.view           = m_view;
    lease.image.extent         = { m_request.width, m_request.height, 1 };
    lease.format               = format();
    lease.initial_layout       = m_defined ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED;
    lease.initial_queue_family = lease.final_queue_family = m_queue_family;
    lease.final_layout                                    = VK_IMAGE_LAYOUT_GENERAL;
    return { .status     = FrameSurfaceAcquireStatus::Acquired,
             .lease      = lease,
             .completion = MakeCompletionCapability(identity) };
}

auto CaptureSurface::CompleteRendered(FrameSurfaceIdentity identity, int producer_sync_fd)
    -> FrameSurfaceCompletionResult {
    if (producer_sync_fd >= 0) ::close(producer_sync_fd);
    m_defined = true;
    if (m_frames++ == m_request.frame) {
        m_captured = true;
        m_on_captured->operator()(read());
    }
    return { .status = FrameSurfaceCompletionStatus::Submitted, .identity = identity };
}

auto CaptureSurface::AbortRenderTarget(FrameSurfaceIdentity identity)
    -> FrameSurfaceCompletionResult {
    return { .status = FrameSurfaceCompletionStatus::Aborted, .identity = identity };
}

auto CaptureSurface::read() -> Option<Vec<u8>> {
    // The renderer submits on this queue from this thread: once it is idle the frame is drawn.
    // Copy the frame (left in GENERAL) to the host-visible buffer.
    if (m_vk.wait_idle(m_queue) != VK_SUCCESS) return None();
    VkCommandBufferBeginInfo begin { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (m_vk.begin(m_commands, &begin) != VK_SUCCESS) return None();
    VkMemoryBarrier drawn { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
    drawn.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    drawn.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    m_vk.barrier(m_commands,
                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                 0,
                 1,
                 &drawn,
                 0,
                 nullptr,
                 0,
                 nullptr);
    VkBufferImageCopy region {};
    region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.imageExtent      = { m_request.width, m_request.height, 1 };
    m_vk.copy(m_commands, m_image, VK_IMAGE_LAYOUT_GENERAL, m_buffer, 1, &region);
    VkMemoryBarrier copied { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
    copied.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    copied.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    m_vk.barrier(m_commands,
                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                 VK_PIPELINE_STAGE_HOST_BIT,
                 0,
                 1,
                 &copied,
                 0,
                 nullptr,
                 0,
                 nullptr);
    if (m_vk.end(m_commands) != VK_SUCCESS) return None();
    VkSubmitInfo submit { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit.commandBufferCount = 1;
    submit.pCommandBuffers    = &m_commands;
    if (m_vk.submit(m_queue, 1, &submit, VK_NULL_HANDLE) != VK_SUCCESS) return None();
    if (m_vk.wait_idle(m_queue) != VK_SUCCESS) return None();

    void* mapped = nullptr;
    if (m_vk.map(m_device, m_buffer_memory, 0, VK_WHOLE_SIZE, 0, &mapped) != VK_SUCCESS)
        return None();
    const auto*  rgba  = static_cast<const rstd::uint8_t*>(mapped);
    const size_t bytes = size_t(m_request.width) * m_request.height * 4;
    Vec<u8>      pixels;
    pixels.reserve(usize(bytes));
    for (size_t index = 0; index < bytes; ++index) pixels.push(u8(rgba[index]));
    m_vk.unmap(m_device, m_buffer_memory);
    return Some(rstd::move(pixels));
}

auto MakeCaptureSurfaceFactory(CaptureRequest request, CaptureCallback on_captured)
    -> Box<dyn<FnOnce<Option<ExSwapchainOwner>(const RenderInitInfo::ExSwapchainHandles&)>>> {
    return Box<dyn<FnOnce<Option<ExSwapchainOwner>(const RenderInitInfo::ExSwapchainHandles&)>>>::
        make([request = rstd::move(request), on_captured = rstd::move(on_captured)](
                 const RenderInitInfo::ExSwapchainHandles& handles) mutable
                 -> Option<ExSwapchainOwner> {
            return Some(ExSwapchain::Make<CaptureSurface>(
                handles, rstd::move(request), rstd::move(on_captured)));
        });
}

} // namespace owe

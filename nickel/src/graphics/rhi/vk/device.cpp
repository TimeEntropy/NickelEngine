#include "graphics/rhi/vk/device.hpp"
#include "graphics/rhi/vk/command.hpp"
#include "graphics/rhi/vk/queue.hpp"
#include "graphics/rhi/vk/util.hpp"

namespace nickel::rhi::vulkan {

DeviceImpl::DeviceImpl(AdapterImpl& adapter) : adapter{adapter} {
    createDevice(adapter.instance, adapter.phyDevice, adapter.surface);
    int w, h;
    SDL_GetWindowSize((SDL_Window*)adapter.window, &w, &h);
    swapchain = Swapchain(adapter.phyDevice, *this, adapter.surface,
                          cgmath::Vec2(w, h));
    createSyncObject();
    createCmdPool();
}

void DeviceImpl::createDevice(vk::Instance instance,
                              vk::PhysicalDevice phyDevice,
                              vk::SurfaceKHR surface) {
    queueIndices =
        chooseQueue(phyDevice, surface, phyDevice.getQueueFamilyProperties());

    if (!queueIndices) {
        LOGW(nickel::log_tag::Vulkan, "no graphics queue in your GPU");
        return;
    }

    vk::DeviceCreateInfo createInfo;
    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;

    std::set<uint32_t> indices{queueIndices.graphicsIndex.value(),
                               queueIndices.presentIndex.value()};

    float priority = 1.0;
    for (auto idx : indices) {
        vk::DeviceQueueCreateInfo createInfo;
        createInfo.setQueueCount(1);
        createInfo.setQueueFamilyIndex(idx);
        createInfo.setQueuePriorities(priority);
        queueCreateInfos.push_back(createInfo);
    }
    createInfo.setQueueCreateInfos(queueCreateInfos);

    std::vector<const char*> requireExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    std::vector<vk::ExtensionProperties> existsExtensions;
    VK_CALL(existsExtensions, phyDevice.enumerateDeviceExtensionProperties());

    using LiteralString = const char*;
    RemoveUnexistsElems<const char*, vk::ExtensionProperties>(
        requireExtensions, existsExtensions,
        [](const LiteralString& e1, const vk::ExtensionProperties& e2) {
            return std::strcmp(e1, e2.extensionName) == 0;
        });

    for (auto ext : requireExtensions) {
        LOGW(nickel::log_tag::Vulkan, "enable extension on device: ", ext);
    }

    createInfo.setPEnabledExtensionNames(requireExtensions);
    auto limits = phyDevice.getFeatures();
    limits.setGeometryShader(true);
    createInfo.setPEnabledFeatures(&limits);

    VK_CALL(device, phyDevice.createDevice(createInfo));
    graphicsQueue = new Queue(new QueueImpl{
        *this, device.getQueue(queueIndices.graphicsIndex.value(), 0)});
    presentQueue = new Queue(new QueueImpl{
        *this, device.getQueue(queueIndices.presentIndex.value(), 0)});
}

DeviceImpl::QueueFamilyIndices DeviceImpl::chooseQueue(
    vk::PhysicalDevice phyDevice, vk::SurfaceKHR surface,
    const std::vector<vk::QueueFamilyProperties>& props) {
    auto queueProps = phyDevice.getQueueFamilyProperties();

    QueueFamilyIndices indices;

    for (int i = 0; i < queueProps.size(); i++) {
        auto& prop = queueProps[i];
        if (prop.queueFlags & vk::QueueFlagBits::eGraphics) {
            indices.graphicsIndex = i;
        }
        bool supportSurface = false;
        VK_CALL(supportSurface, phyDevice.getSurfaceSupportKHR(i, surface));
        if (supportSurface) {
            indices.presentIndex = i;
        }

        if (indices) {
            break;
        }
    }

    return indices;
}

void DeviceImpl::createCmdPool() {
    vk::CommandPoolCreateInfo info;
    info.setFlags(vk::CommandPoolCreateFlagBits::eTransient)
        .setQueueFamilyIndex(queueIndices.graphicsIndex.value());

    VK_CALL(cmdPool, device.createCommandPool(info));
}

void DeviceImpl::createSyncObject() {
    for (int i = 0; i < swapchain.images.size(); i++) {
        vk::FenceCreateInfo info;
        info.setFlags(vk::FenceCreateFlagBits::eSignaled);
        vk::Fence fence;
        VK_CALL(fence, device.createFence(info));
        fences.emplace_back(fence);

        vk::SemaphoreCreateInfo semInfo;
        vk::Semaphore sem;
        VK_CALL(sem, device.createSemaphore(semInfo));
        imageAvaliableSems.emplace_back(sem);
        VK_CALL(sem, device.createSemaphore(semInfo));
        renderFinishSems.emplace_back(sem);
    }
}

DeviceImpl::~DeviceImpl() {
    VK_CALL_NO_VALUE(device.waitIdle());

    for (auto fence : fences) {
        device.destroyFence(fence);
    }
    for (auto sem : imageAvaliableSems) {
        device.destroySemaphore(sem);
    }
    for (auto sem : renderFinishSems) {
        device.destroySemaphore(sem);
    }

    for (auto renderPass : renderPasses) {
        renderPass.Destroy();
    }

    for (auto fbo : framebuffers) {
        fbo.Destroy();
    }

    device.destroyCommandPool(cmdPool);
    swapchain.Destroy(device);
    delete graphicsQueue;
    delete presentQueue;
    device.destroy();
}

Texture DeviceImpl::CreateTexture(const Texture::Descriptor& desc) {
    if (queueIndices.HasSeperateQueue()) {
        return Texture(adapter, *this, desc,
                       {queueIndices.graphicsIndex.value(),
                        queueIndices.presentIndex.value()});
    } else {
        return Texture(adapter, *this, desc,
                       {queueIndices.graphicsIndex.value()});
    }
}

RenderPipeline DeviceImpl::CreateRenderPipeline(
    const RenderPipeline::Descriptor& desc) {
    return RenderPipeline{APIPreference::Vulkan, *this, desc};
}

PipelineLayout DeviceImpl::CreatePipelineLayout(
    const PipelineLayout::Descriptor& desc) {
    return PipelineLayout{APIPreference::Vulkan, *this, desc};
}

CommandEncoder DeviceImpl::CreateCommandEncoder() {
    return CommandEncoder{
        new vulkan::CommandEncoderImpl{*this, cmdPool}
    };
}

Sampler DeviceImpl::CreateSampler(const Sampler::Descriptor& desc) {
    return Sampler{APIPreference::Vulkan, *this, desc};
}

BindGroup DeviceImpl::CreateBindGroup(const BindGroup::Descriptor& desc) {
    return BindGroup{APIPreference::Vulkan, *this, desc};
}

BindGroupLayout DeviceImpl::CreateBindGroupLayout(
    const BindGroupLayout::Descriptor& desc) {
    return BindGroupLayout{APIPreference::Vulkan, *this, desc};
}

void DeviceImpl::OnWindowResize(const cgmath::Vec2& size) {
    swapchain.Destroy(device);
    swapchain = Swapchain(adapter.phyDevice, *this, adapter.surface, size);

    // remove renderPass which contains swapchain image
    {
        auto it = std::remove_if(
            renderPasses.begin(), renderPasses.end(),
            [](RenderPass& renderPass) {
                for (auto& att : renderPass.GetDescriptor().colorAttachments) {
                    if (att.view.Format() == rhi::TextureFormat::Presentation) {
                        renderPass.Destroy();
                        return true;
                    }
                }
                return false;
            });
        renderPasses.erase(it, renderPasses.end());
    }

    // remove framebuffer which contains swapchain image
    {
        auto it = std::remove_if(
            framebuffers.begin(), framebuffers.end(), [&](Framebuffer& fbo) {
                auto impl = static_cast<FramebufferImpl*>(fbo.Impl());
                for (auto& att : impl->Views()) {
                    for (auto& image : swapchain.ImageViews()) {
                        if (att == image) {
                            fbo.Destroy();
                            return true;
                        }
                    }
                }
                return false;
            });
        framebuffers.erase(it, framebuffers.end());
    }
}

void DeviceImpl::BeginFrame() {
    auto window = (SDL_Window*)adapter.window;
    if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
        return;
    }

    VK_CALL(curImageIndex,
            device.acquireNextImageKHR(swapchain.swapchain, UINT64_MAX,
                                       imageAvaliableSems[curFrame]));
}

void DeviceImpl::EndFrame() {
    cmdCounter.Reset();

    auto window = (SDL_Window*)adapter.window;
    if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
        VK_CALL_NO_VALUE(
            device.waitForFences(fences[curFrame], true, UINT64_MAX));
        return;
    }

    // transform present image barrier
    vk::CommandBufferAllocateInfo allocInfo;
    allocInfo.setCommandBufferCount(1).setCommandPool(cmdPool).setLevel(
        vk::CommandBufferLevel::ePrimary);
    std::vector<vk::CommandBuffer> cmdBufs;
    VK_CALL(cmdBufs, device.allocateCommandBuffers(allocInfo));
    vk::ImageMemoryBarrier barrier;
    vk::ImageSubresourceRange range;
    range.setAspectMask(vk::ImageAspectFlagBits::eColor)
        .setBaseArrayLayer(0)
        .setLayerCount(1)
        .setBaseMipLevel(0)
        .setLevelCount(1);

    barrier.setOldLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setNewLayout(vk::ImageLayout::ePresentSrcKHR)
        .setSrcAccessMask(vk::AccessFlagBits::eNone)
        .setDstAccessMask(vk::AccessFlagBits::eNone)
        .setImage(swapchain.Images()[curImageIndex])
        .setSubresourceRange(range);

    vk::CommandBufferBeginInfo beginInfo;
    beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    VK_CALL_NO_VALUE(cmdBufs[0].begin(beginInfo));
    cmdBufs[0].pipelineBarrier(vk::PipelineStageFlagBits::eBottomOfPipe,
                               vk::PipelineStageFlagBits::eBottomOfPipe,
                               vk::DependencyFlagBits::eByRegion, {}, {},
                               barrier);
    VK_CALL_NO_VALUE(cmdBufs[0].end());

    vk::Queue graphics =
        static_cast<const vulkan::QueueImpl*>(graphicsQueue->Impl())->queue;
    vk::Queue present =
        static_cast<const vulkan::QueueImpl*>(presentQueue->Impl())->queue;

    vk::SubmitInfo submit;
    submit.setCommandBuffers(cmdBufs);
    VK_CALL_NO_VALUE(graphics.submit(submit));
    VK_CALL_NO_VALUE(device.waitIdle());

    device.freeCommandBuffers(cmdPool, cmdBufs);

    vk::PresentInfoKHR info;
    info.setImageIndices(curImageIndex)
        .setWaitSemaphores(renderFinishSems[curFrame])
        .setSwapchains(swapchain.swapchain);
    VK_CALL_NO_VALUE(present.presentKHR(info));

    device.resetCommandPool(cmdPool);

    curFrame = (curFrame + 1) % swapchain.imageInfo.imagCount;
}

ShaderModule DeviceImpl::CreateShaderModule(
    const ShaderModule::Descriptor& desc) {
    return ShaderModule{APIPreference::Vulkan, *this, desc};
}

void DeviceImpl::WaitIdle() {
    VK_CALL_NO_VALUE(device.waitIdle());
}

Buffer DeviceImpl::CreateBuffer(const Buffer::Descriptor& desc) {
    return Buffer{adapter, *this, desc};
}

Queue DeviceImpl::GetQueue() {
    return *graphicsQueue;
}

}  // namespace nickel::rhi::vulkan
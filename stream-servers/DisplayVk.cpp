#include "DisplayVk.h"

#include <algorithm>
#include <chrono>
#include <glm/glm.hpp>
#include <glm/gtx/matrix_transform_2d.hpp>

#define DISPLAY_VK_ERROR(fmt, ...)                                                            \
    do {                                                                                      \
        fprintf(stderr, "%s(%s:%d): " fmt "\n", __func__, __FILE__, __LINE__, ##__VA_ARGS__); \
        fflush(stderr);                                                                       \
    } while (0)

namespace {

bool shouldRecreateSwapchain(VkResult result) {
    return result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR;
}

}  // namespace

DisplayVk::DisplayVk(const goldfish_vk::VulkanDispatch &vk, VkPhysicalDevice vkPhysicalDevice,
                     uint32_t swapChainQueueFamilyIndex, uint32_t compositorQueueFamilyIndex,
                     VkDevice vkDevice, VkQueue compositorVkQueue,
                     std::shared_ptr<android::base::Lock> compositorVkQueueLock,
                     VkQueue swapChainVkqueue,
                     std::shared_ptr<android::base::Lock> swapChainVkQueueLock)
    : m_vk(vk),
      m_vkPhysicalDevice(vkPhysicalDevice),
      m_swapChainQueueFamilyIndex(swapChainQueueFamilyIndex),
      m_compositorQueueFamilyIndex(compositorQueueFamilyIndex),
      m_vkDevice(vkDevice),
      m_compositorVkQueue(compositorVkQueue),
      m_compositorVkQueueLock(compositorVkQueueLock),
      m_swapChainVkQueue(swapChainVkqueue),
      m_swapChainVkQueueLock(swapChainVkQueueLock),
      m_vkCommandPool(VK_NULL_HANDLE),
      m_swapChainStateVk(nullptr),
      m_compositorVk(nullptr),
      m_surfaceState(nullptr),
      m_canComposite() {
    // TODO(kaiyili): validate the capabilites of the passed in Vulkan
    // components.
    VkCommandPoolCreateInfo commandPoolCi = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = m_compositorQueueFamilyIndex,
    };
    VK_CHECK(m_vk.vkCreateCommandPool(m_vkDevice, &commandPoolCi, nullptr, &m_vkCommandPool));

    VkSamplerCreateInfo samplerCi = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                     .magFilter = VK_FILTER_LINEAR,
                                     .minFilter = VK_FILTER_LINEAR,
                                     .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                                     .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                                     .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                                     .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                                     .mipLodBias = 0.0f,
                                     .anisotropyEnable = VK_FALSE,
                                     .maxAnisotropy = 1.0f,
                                     .compareEnable = VK_FALSE,
                                     .compareOp = VK_COMPARE_OP_ALWAYS,
                                     .minLod = 0.0f,
                                     .maxLod = 0.0f,
                                     .borderColor = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK,
                                     .unnormalizedCoordinates = VK_FALSE};
    VK_CHECK(m_vk.vkCreateSampler(m_vkDevice, &samplerCi, nullptr, &m_compositionVkSampler));
}

DisplayVk::~DisplayVk() {
    {
        android::base::AutoLock lock(*m_swapChainVkQueueLock);
        VK_CHECK(m_vk.vkQueueWaitIdle(m_swapChainVkQueue));
    }
    {
        android::base::AutoLock lock(*m_compositorVkQueueLock);
        VK_CHECK(m_vk.vkQueueWaitIdle(m_compositorVkQueue));
    }
    m_composeResourceFuture = std::nullopt;
    m_vk.vkDestroySampler(m_vkDevice, m_compositionVkSampler, nullptr);
    m_surfaceState.reset();
    m_compositorVk.reset();
    m_swapChainStateVk.reset();
    m_vk.vkDestroyCommandPool(m_vkDevice, m_vkCommandPool, nullptr);
}

void DisplayVk::bindToSurface(VkSurfaceKHR surface, uint32_t width, uint32_t height) {
    {
        android::base::AutoLock lock(*m_compositorVkQueueLock);
        VK_CHECK(m_vk.vkQueueWaitIdle(m_compositorVkQueue));
    }
    {
        android::base::AutoLock lock(*m_swapChainVkQueueLock);
        VK_CHECK(m_vk.vkQueueWaitIdle(m_swapChainVkQueue));
    }
    m_composeResourceFuture = std::nullopt;
    m_compositorVk.reset();
    m_swapChainStateVk.reset();

    if (!SwapChainStateVk::validateQueueFamilyProperties(m_vk, m_vkPhysicalDevice, surface,
                                                         m_swapChainQueueFamilyIndex)) {
        DISPLAY_VK_ERROR(
            "DisplayVk can't create VkSwapchainKHR with given VkDevice and VkSurfaceKHR.");
        ::abort();
    }
    auto swapChainCi = SwapChainStateVk::createSwapChainCi(
        m_vk, surface, m_vkPhysicalDevice, width, height,
        {m_swapChainQueueFamilyIndex, m_compositorQueueFamilyIndex});
    VkFormatProperties formatProps;
    m_vk.vkGetPhysicalDeviceFormatProperties(m_vkPhysicalDevice, swapChainCi->imageFormat,
                                             &formatProps);
    if (!(formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT)) {
        DISPLAY_VK_ERROR(
            "DisplayVk: The image format chosen for present VkImage can't be used as the color "
            "attachment, and therefore can't be used as the render target of CompositorVk.");
        ::abort();
    }
    m_swapChainStateVk = std::make_unique<SwapChainStateVk>(m_vk, m_vkDevice, *swapChainCi);
    m_compositorVk = CompositorVk::create(
        m_vk, m_vkDevice, m_vkPhysicalDevice, m_compositorVkQueue, m_compositorVkQueueLock,
        m_swapChainStateVk->getFormat(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        m_swapChainStateVk->getVkImageViews().size(), m_vkCommandPool, m_compositionVkSampler);

    m_composeResourceFuture = std::async(std::launch::deferred, [this] {
                                  return ComposeResource::create(m_vk, m_vkDevice, m_vkCommandPool);
                              }).share();
    m_composeResourceFuture.value().wait();

    auto surfaceState = std::make_unique<SurfaceState>();
    surfaceState->m_height = height;
    surfaceState->m_width = width;
    for (VkImageView imageView : m_swapChainStateVk->getVkImageViews()) {
        surfaceState->m_renderTargets.emplace_back(
            m_compositorVk->createRenderTarget(imageView, width, height));
    }
    m_surfaceState = std::move(surfaceState);
}

std::shared_ptr<DisplayVk::DisplayBufferInfo> DisplayVk::createDisplayBuffer(VkImage image,
                                                                             VkFormat format,
                                                                             uint32_t width,
                                                                             uint32_t height) {
    return std::shared_ptr<DisplayBufferInfo>(
        new DisplayBufferInfo(m_vk, m_vkDevice, width, height, format, image));
}

std::tuple<bool, std::shared_future<void>> DisplayVk::post(
    const std::shared_ptr<DisplayBufferInfo> &displayBufferPtr) {
    auto completedFuture = std::async(std::launch::deferred, [] {}).share();
    completedFuture.wait();
    if (!displayBufferPtr) {
        fprintf(stderr, "%s: warning: null ptr passed to post buffer\n", __func__);
        return std::make_tuple(true, std::move(completedFuture));
    }
    // TODO(kaiyili): According to VUID-vkCmdBlitImage-srcImage-01999, check if the format feature
    // of displayBufferPtr->m_vkImage contains VK_FORMAT_FEATURE_BLIT_SRC_BIT.
    // TODO(kaiyili): According to VUID-vkCmdBlitImage-srcImage-06421, check if the format of
    // displayBufferPtr->m_vkImage requires a sampler Ycbcr sampler.
    // TODO(kaiyili): According to VUID-vkCmdBlitImage-srcImage-00219, check if
    // displayBufferPtr->m_vkImage is created with VK_IMAGE_USAGE_TRANSFER_SRC_BIT usage flag.
    // TODO(kaiyili): According to VUID-vkCmdBlitImage-srcImage-00229,
    // VUID-vkCmdBlitImage-srcImage-00230, check if the signedness of the formats of
    // displayBufferPtr->m_vkImage and the presentable images are consistent.
    // TODO(kaiyili): According to VUID-vkCmdBlitImage-srcImage-00233, check if
    // displayBufferPtr->m_vkImage is created with a samples value of VK_SAMPLE_COUNT_1_BIT.
    // TODO(kaiyili): According to VUID-vkCmdBlitImage-dstImage-02545, check if
    // displayBufferPtr->m_vkImage is created iwth flags containing
    // VK_IMAGE_CREATE_SUBSAMPLED_BIT_EXT.
    if (!m_swapChainStateVk || !m_surfaceState) {
        DISPLAY_VK_ERROR("Haven't bound to a surface, can't post ColorBuffer.");
        return std::make_tuple(true, std::move(completedFuture));
    }

    std::shared_ptr<ComposeResource> composeResource = m_composeResourceFuture.value().get();

    VkSemaphore imageReadySem = composeResource->m_swapchainImageAcquireSemaphore;

    uint32_t imageIndex;
    VkResult acquireRes =
        m_vk.vkAcquireNextImageKHR(m_vkDevice, m_swapChainStateVk->getSwapChain(), UINT64_MAX,
                                   imageReadySem, VK_NULL_HANDLE, &imageIndex);
    if (shouldRecreateSwapchain(acquireRes)) {
        return std::make_tuple(false, std::shared_future<void>());
    }
    VK_CHECK(acquireRes);

    VkCommandBuffer cmdBuff = composeResource->m_vkCommandBuffer;
    VK_CHECK(m_vk.vkResetCommandBuffer(cmdBuff, 0));
    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    VK_CHECK(m_vk.vkBeginCommandBuffer(cmdBuff, &beginInfo));
    VkImageMemoryBarrier presentToXferDstBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = m_swapChainStateVk->getVkImages()[imageIndex],
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1}};
    m_vk.vkCmdPipelineBarrier(cmdBuff, VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                              &presentToXferDstBarrier);
    VkImageBlit region = {
        .srcSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                           .mipLevel = 0,
                           .baseArrayLayer = 0,
                           .layerCount = 1},
        .srcOffsets = {{0, 0, 0},
                       {static_cast<int32_t>(displayBufferPtr->m_width),
                        static_cast<int32_t>(displayBufferPtr->m_height), 1}},
        .dstSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                           .mipLevel = 0,
                           .baseArrayLayer = 0,
                           .layerCount = 1},
        .dstOffsets = {{0, 0, 0},
                       {static_cast<int32_t>(m_surfaceState->m_width),
                        static_cast<int32_t>(m_surfaceState->m_height), 1}},
    };
    // TODO(kaiyili), use VK_FILTER_LINEAR if the format features of displayBufferPtr->m_vkImage
    // contain VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT.
    m_vk.vkCmdBlitImage(cmdBuff, displayBufferPtr->m_vkImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        m_swapChainStateVk->getVkImages()[imageIndex],
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, VK_FILTER_NEAREST);
    VkImageMemoryBarrier xferDstToPresentBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = m_swapChainStateVk->getVkImages()[imageIndex],
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1}};
    m_vk.vkCmdPipelineBarrier(cmdBuff, VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1,
                              &xferDstToPresentBarrier);
    VK_CHECK(m_vk.vkEndCommandBuffer(cmdBuff));

    VkFence postCompleteFence = composeResource->m_swapchainImageReleaseFence;
    VK_CHECK(m_vk.vkResetFences(m_vkDevice, 1, &postCompleteFence));
    VkSemaphore postCompleteSemaphore = composeResource->m_swapchainImageReleaseSemaphore;
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_TRANSFER_BIT};
    VkSubmitInfo submitInfo = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                               .waitSemaphoreCount = 1,
                               .pWaitSemaphores = &imageReadySem,
                               .pWaitDstStageMask = waitStages,
                               .commandBufferCount = 1,
                               .pCommandBuffers = &cmdBuff,
                               .signalSemaphoreCount = 1,
                               .pSignalSemaphores = &postCompleteSemaphore};
    {
        android::base::AutoLock lock(*m_compositorVkQueueLock);
        VK_CHECK(m_vk.vkQueueSubmit(m_compositorVkQueue, 1, &submitInfo, postCompleteFence));
    }
    std::shared_future<std::shared_ptr<ComposeResource>> composeResourceFuture =
        std::async(std::launch::deferred, [postCompleteFence, composeResource, this] {
            VK_CHECK(m_vk.vkWaitForFences(m_vkDevice, 1, &postCompleteFence, VK_TRUE, UINT64_MAX));
            return composeResource;
        }).share();
    m_composeResourceFuture = composeResourceFuture;

    auto swapChain = m_swapChainStateVk->getSwapChain();
    VkPresentInfoKHR presentInfo = {.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                    .waitSemaphoreCount = 1,
                                    .pWaitSemaphores = &postCompleteSemaphore,
                                    .swapchainCount = 1,
                                    .pSwapchains = &swapChain,
                                    .pImageIndices = &imageIndex};
    VkResult presentRes;
    {
        android::base::AutoLock lock(*m_swapChainVkQueueLock);
        presentRes = m_vk.vkQueuePresentKHR(m_swapChainVkQueue, &presentInfo);
    }
    if (shouldRecreateSwapchain(presentRes)) {
        composeResourceFuture.wait();
        return std::make_tuple(false, std::shared_future<void>());
    }
    VK_CHECK(presentRes);
    return std::make_tuple(true, std::async(std::launch::deferred, [composeResourceFuture] {
                                     // We can't directly wait for the VkFence here, because we
                                     // share the VkFences on different frames, but we don't share
                                     // the future on different frames. If we directly wait for the
                                     // VkFence here, we may wait for a different frame if a new
                                     // frame starts to be drawn before this future is waited.
                                     composeResourceFuture.wait();
                                 }).share());
}

std::tuple<bool, std::shared_future<void>> DisplayVk::compose(
    uint32_t numLayers, const ComposeLayer layers[],
    const std::vector<std::shared_ptr<DisplayBufferInfo>> &composeBuffers, uint32_t dstWidth,
    uint32_t dstHeight) {
    std::shared_future<void> completedFuture = std::async(std::launch::deferred, [] {}).share();
    completedFuture.wait();

    if (!m_swapChainStateVk || !m_compositorVk) {
        DISPLAY_VK_ERROR("Haven't bound to a surface, can't compose color buffer.");
        // The surface hasn't been created yet, hence we don't return
        // std::nullopt to request rebinding.
        return std::make_tuple(true, std::move(completedFuture));
    }

    std::vector<std::unique_ptr<ComposeLayerVk>> composeLayers;
    for (int i = 0; i < numLayers; ++i) {
        if (layers[i].cbHandle == 0) {
            // When ColorBuffer handle is 0, it's expected that no ColorBuffer
            // is not found.
            continue;
        }
        if (!composeBuffers[i]) {
            fprintf(stderr, "%s: warning: null ptr passed to compose buffer for layer %d\n",
                    __func__, i);
            continue;
        }
        const auto &db = *composeBuffers[i];
        if (!canComposite(db.m_vkFormat)) {
            DISPLAY_VK_ERROR("Can't composite the DisplayBuffer(0x%" PRIxPTR
                             "). The image(VkFormat = %" PRIu64 ") can't be sampled from.",
                             reinterpret_cast<uintptr_t>(&db),
                             static_cast<uint64_t>(db.m_vkFormat));
            continue;
        }
        auto layer = ComposeLayerVk::createFromHwc2ComposeLayer(
            m_compositionVkSampler, composeBuffers[i]->m_vkImageView, layers[i],
            composeBuffers[i]->m_width, composeBuffers[i]->m_height, dstWidth, dstHeight);
        composeLayers.emplace_back(std::move(layer));
    }

    if (composeLayers.empty()) {
        return std::make_tuple(true, std::move(completedFuture));
    }

    std::shared_ptr<ComposeResource> composeResource = m_composeResourceFuture.value().get();

    VkSemaphore imageReadySem = composeResource->m_swapchainImageAcquireSemaphore;
    VkSemaphore frameDrawCompleteSem = composeResource->m_swapchainImageReleaseSemaphore;

    uint32_t imageIndex;
    VkResult acquireRes =
        m_vk.vkAcquireNextImageKHR(m_vkDevice, m_swapChainStateVk->getSwapChain(), UINT64_MAX,
                                   imageReadySem, VK_NULL_HANDLE, &imageIndex);
    if (shouldRecreateSwapchain(acquireRes)) {
        return std::make_tuple(false, std::shared_future<void>());
    }
    VK_CHECK(acquireRes);

    VkFence frameDrawCompleteFence = composeResource->m_swapchainImageReleaseFence;
    if (compareAndSaveComposition(imageIndex, numLayers, layers, composeBuffers)) {
        auto composition = std::make_unique<Composition>(std::move(composeLayers));
        m_compositorVk->setComposition(imageIndex, std::move(composition));
    }

    VkCommandBuffer cmdBuff = composeResource->m_vkCommandBuffer;
    VK_CHECK(m_vk.vkResetCommandBuffer(cmdBuff, 0));

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    VK_CHECK(m_vk.vkBeginCommandBuffer(cmdBuff, &beginInfo));
    m_compositorVk->recordCommandBuffers(imageIndex, cmdBuff,
                                         *m_surfaceState->m_renderTargets[imageIndex]);
    VK_CHECK(m_vk.vkEndCommandBuffer(cmdBuff));

    VK_CHECK(m_vk.vkResetFences(m_vkDevice, 1, &frameDrawCompleteFence));
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSubmitInfo submitInfo = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                               .waitSemaphoreCount = 1,
                               .pWaitSemaphores = &imageReadySem,
                               .pWaitDstStageMask = waitStages,
                               .commandBufferCount = 1,
                               .pCommandBuffers = &cmdBuff,
                               .signalSemaphoreCount = 1,
                               .pSignalSemaphores = &frameDrawCompleteSem};
    {
        android::base::AutoLock lock(*m_compositorVkQueueLock);
        VK_CHECK(m_vk.vkQueueSubmit(m_compositorVkQueue, 1, &submitInfo, frameDrawCompleteFence));
    }

    std::shared_future<std::shared_ptr<ComposeResource>> composeResourceFuture =
        std::async(std::launch::deferred, [frameDrawCompleteFence, composeResource, this] {
            VK_CHECK(
                m_vk.vkWaitForFences(m_vkDevice, 1, &frameDrawCompleteFence, VK_TRUE, UINT64_MAX));
            return composeResource;
        }).share();

    std::shared_future<void> frameDrawCompleteFuture =
        std::async(std::launch::deferred, [composeResourceFuture] {
            // We can't directly wait for the VkFence here, because we share the VkFences on
            // different frames, but we don't share the future on different frames. If we directly
            // wait for the VkFence here, we may wait for a different frame if a new frame starts to
            // be drawn before this future is waited.
            composeResourceFuture.wait();
        }).share();
    m_composeResourceFuture = composeResourceFuture;

    auto swapChain = m_swapChainStateVk->getSwapChain();
    VkPresentInfoKHR presentInfo = {.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                    .waitSemaphoreCount = 1,
                                    .pWaitSemaphores = &frameDrawCompleteSem,
                                    .swapchainCount = 1,
                                    .pSwapchains = &swapChain,
                                    .pImageIndices = &imageIndex};
    VkResult presentRes;
    {
        android::base::AutoLock lock(*m_swapChainVkQueueLock);
        presentRes = m_vk.vkQueuePresentKHR(m_swapChainVkQueue, &presentInfo);
    }
    if (shouldRecreateSwapchain(presentRes)) {
        frameDrawCompleteFuture.wait();
        return std::make_tuple(false, std::shared_future<void>());
    }
    VK_CHECK(presentRes);
    return std::make_tuple(true, frameDrawCompleteFuture);
}

bool DisplayVk::canComposite(VkFormat format) {
    auto it = m_canComposite.find(format);
    if (it != m_canComposite.end()) {
        return it->second;
    }
    VkFormatProperties formatProps = {};
    m_vk.vkGetPhysicalDeviceFormatProperties(m_vkPhysicalDevice, format, &formatProps);
    bool res = formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
    m_canComposite.emplace(format, res);
    return res;
}

bool DisplayVk::compareAndSaveComposition(
    uint32_t renderTargetIndex, uint32_t numLayers, const ComposeLayer layers[],
    const std::vector<std::shared_ptr<DisplayBufferInfo>> &composeBuffers) {
    if (!m_surfaceState) {
        DISPLAY_VK_ERROR("Haven't bound to a surface, can't compare and save composition.");
        ::abort();
    }
    auto [iPrevComposition, compositionNotFound] =
        m_surfaceState->m_prevCompositions.emplace(renderTargetIndex, 0);
    auto &prevComposition = iPrevComposition->second;
    bool compositionChanged = false;
    if (numLayers == prevComposition.size()) {
        for (int i = 0; i < numLayers; i++) {
            if (composeBuffers[i] == nullptr) {
                // If the display buffer of the current layer doesn't exist, we
                // check if the layer at the same index in the previous
                // composition doesn't exist either.
                if (prevComposition[i] == nullptr) {
                    continue;
                } else {
                    compositionChanged = true;
                    break;
                }
            }
            if (prevComposition[i] == nullptr) {
                // If the display buffer of the current layer exists but the
                // layer at the same index in the previous composition doesn't
                // exist, the composition is changed.
                compositionChanged = true;
                break;
            }
            const auto &prevLayer = *prevComposition[i];
            const auto prevDisplayBufferPtr = prevLayer.m_displayBuffer.lock();
            // prevLayer.m_displayBuffer is a weak pointer, so if
            // prevDisplayBufferPtr is null, the color buffer
            // prevDisplayBufferPtr pointed to should have been released or
            // re-allocated, and we should consider the composition is changed.
            // If prevDisplayBufferPtr exists and it points to the same display
            // buffer as the input composeBuffers[i] we consider the composition
            // not changed.
            if (!prevDisplayBufferPtr || prevDisplayBufferPtr != composeBuffers[i]) {
                compositionChanged = true;
                break;
            }
            const auto &prevHwc2Layer = prevLayer.m_hwc2Layer;
            const auto hwc2Layer = layers[i];
            compositionChanged =
                (prevHwc2Layer.cbHandle != hwc2Layer.cbHandle) ||
                (prevHwc2Layer.composeMode != hwc2Layer.composeMode) ||
                (prevHwc2Layer.displayFrame.left != hwc2Layer.displayFrame.left) ||
                (prevHwc2Layer.displayFrame.top != hwc2Layer.displayFrame.top) ||
                (prevHwc2Layer.displayFrame.right != hwc2Layer.displayFrame.right) ||
                (prevHwc2Layer.displayFrame.bottom != hwc2Layer.displayFrame.bottom) ||
                (prevHwc2Layer.crop.left != hwc2Layer.crop.left) ||
                (prevHwc2Layer.crop.top != hwc2Layer.crop.top) ||
                (prevHwc2Layer.crop.right != hwc2Layer.crop.right) ||
                (prevHwc2Layer.crop.bottom != hwc2Layer.crop.bottom) ||
                (prevHwc2Layer.blendMode != hwc2Layer.blendMode) ||
                (prevHwc2Layer.alpha != hwc2Layer.alpha) ||
                (prevHwc2Layer.color.r != hwc2Layer.color.r) ||
                (prevHwc2Layer.color.g != hwc2Layer.color.g) ||
                (prevHwc2Layer.color.b != hwc2Layer.color.b) ||
                (prevHwc2Layer.color.a != hwc2Layer.color.a) ||
                (prevHwc2Layer.transform != hwc2Layer.transform);
            if (compositionChanged) {
                break;
            }
        }
    } else {
        compositionChanged = true;
    }
    bool needsSave = compositionNotFound || compositionChanged;
    if (needsSave) {
        prevComposition.clear();
        for (int i = 0; i < numLayers; i++) {
            if (composeBuffers[i] == nullptr) {
                prevComposition.emplace_back(nullptr);
                continue;
            }
            auto layer = std::make_unique<SurfaceState::Layer>();
            layer->m_hwc2Layer = layers[i];
            layer->m_displayBuffer = composeBuffers[i];
            prevComposition.emplace_back(std::move(layer));
        }
    }
    return needsSave;
}

DisplayVk::DisplayBufferInfo::DisplayBufferInfo(const goldfish_vk::VulkanDispatch &vk,
                                                VkDevice vkDevice, uint32_t width, uint32_t height,
                                                VkFormat format, VkImage image)
    : m_vk(vk),
      m_vkDevice(vkDevice),
      m_width(width),
      m_height(height),
      m_vkFormat(format),
      m_vkImage(image),
      m_vkImageView(VK_NULL_HANDLE) {
    VkImageViewCreateInfo imageViewCi = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .components = {.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                       .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                       .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                       .a = VK_COMPONENT_SWIZZLE_IDENTITY},
        .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                             .baseMipLevel = 0,
                             .levelCount = 1,
                             .baseArrayLayer = 0,
                             .layerCount = 1}};
    VK_CHECK(m_vk.vkCreateImageView(m_vkDevice, &imageViewCi, nullptr, &m_vkImageView));
}

DisplayVk::DisplayBufferInfo::~DisplayBufferInfo() {
    m_vk.vkDestroyImageView(m_vkDevice, m_vkImageView, nullptr);
}

std::shared_ptr<DisplayVk::ComposeResource> DisplayVk::ComposeResource::create(
    const goldfish_vk::VulkanDispatch &vk, VkDevice vkDevice, VkCommandPool vkCommandPool) {
    VkFenceCreateInfo fenceCi = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    };
    VkFence fence;
    VK_CHECK(vk.vkCreateFence(vkDevice, &fenceCi, nullptr, &fence));
    VkSemaphore semaphores[2];
    for (uint32_t i = 0; i < std::size(semaphores); i++) {
        VkSemaphoreCreateInfo semaphoreCi = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        };
        VK_CHECK(vk.vkCreateSemaphore(vkDevice, &semaphoreCi, nullptr, &semaphores[i]));
    }
    VkCommandBuffer commandBuffer;
    VkCommandBufferAllocateInfo commandBufferAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = vkCommandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VK_CHECK(vk.vkAllocateCommandBuffers(vkDevice, &commandBufferAllocInfo, &commandBuffer));
    return std::shared_ptr<ComposeResource>(new ComposeResource(
        vk, vkDevice, vkCommandPool, fence, semaphores[0], semaphores[1], commandBuffer));
}

DisplayVk::ComposeResource::~ComposeResource() {
    m_vk.vkFreeCommandBuffers(m_vkDevice, m_vkCommandPool, 1, &m_vkCommandBuffer);
    m_vk.vkDestroyFence(m_vkDevice, m_swapchainImageReleaseFence, nullptr);
    m_vk.vkDestroySemaphore(m_vkDevice, m_swapchainImageAcquireSemaphore, nullptr);
    m_vk.vkDestroySemaphore(m_vkDevice, m_swapchainImageReleaseSemaphore, nullptr);
}

DisplayVk::ComposeResource::ComposeResource(const goldfish_vk::VulkanDispatch &vk,
                                            VkDevice vkDevice, VkCommandPool vkCommandPool,
                                            VkFence swapchainImageReleaseFence,
                                            VkSemaphore swapchainImageAcquireSemaphore,
                                            VkSemaphore swapchainImageReleaseSemaphore,
                                            VkCommandBuffer vkCommandBuffer)
    : m_swapchainImageReleaseFence(swapchainImageReleaseFence),
      m_swapchainImageAcquireSemaphore(swapchainImageAcquireSemaphore),
      m_swapchainImageReleaseSemaphore(swapchainImageReleaseSemaphore),
      m_vkCommandBuffer(vkCommandBuffer),
      m_vk(vk),
      m_vkDevice(vkDevice),
      m_vkCommandPool(vkCommandPool) {}

#ifndef DISPLAY_VK_H
#define DISPLAY_VK_H

#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

#include "BorrowedImage.h"
#include "CompositorVk.h"
#include "Display.h"
#include "DisplaySurfaceVk.h"
#include "Hwc2.h"
#include "SwapChainStateVk.h"
#include "base/synchronization/Lock.h"
#include "vulkan/cereal/common/goldfish_vk_dispatch.h"

// The DisplayVk class holds the Vulkan and other states required to draw a
// frame in a host window.

class DisplayVk : public gfxstream::Display {
   public:
    DisplayVk(const goldfish_vk::VulkanDispatch&, VkPhysicalDevice,
              uint32_t swapChainQueueFamilyIndex, uint32_t compositorQueueFamilyIndex, VkDevice,
              VkQueue compositorVkQueue, std::shared_ptr<android::base::Lock> compositorVkQueueLock,
              VkQueue swapChainVkQueue, std::shared_ptr<android::base::Lock> swapChainVkQueueLock);
    ~DisplayVk();

    std::shared_future<void> post(const BorrowedImageInfo* info);

    void drainQueues();

   protected:
    void bindToSurfaceImpl(gfxstream::DisplaySurface* surface) override;
    void unbindFromSurfaceImpl() override;

   private:
    bool recreateSwapchain();

    // The first component of the returned tuple is false when the swapchain is no longer valid and
    // bindToSurface() needs to be called again. When the first component is true, the second
    // component of the returned tuple is a/ future that will complete when the GPU side of work
    // completes. The caller is responsible to guarantee the synchronization and the layout of
    // ColorBufferCompositionInfo::m_vkImage is VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL.
    std::tuple<bool, std::shared_future<void>> postImpl(const BorrowedImageInfo* info);

    VkFormatFeatureFlags getFormatFeatures(VkFormat, VkImageTiling);
    bool canPost(const VkImageCreateInfo&);

    const goldfish_vk::VulkanDispatch& m_vk;
    VkPhysicalDevice m_vkPhysicalDevice;
    uint32_t m_swapChainQueueFamilyIndex;
    uint32_t m_compositorQueueFamilyIndex;
    VkDevice m_vkDevice;
    VkQueue m_compositorVkQueue;
    std::shared_ptr<android::base::Lock> m_compositorVkQueueLock;
    VkQueue m_swapChainVkQueue;
    std::shared_ptr<android::base::Lock> m_swapChainVkQueueLock;
    VkCommandPool m_vkCommandPool;

    class PostResource {
       public:
        const VkFence m_swapchainImageReleaseFence;
        const VkSemaphore m_swapchainImageAcquireSemaphore;
        const VkSemaphore m_swapchainImageReleaseSemaphore;
        const VkCommandBuffer m_vkCommandBuffer;
        static std::shared_ptr<PostResource> create(const goldfish_vk::VulkanDispatch&, VkDevice,
                                                    VkCommandPool);
        ~PostResource();
        DISALLOW_COPY_ASSIGN_AND_MOVE(PostResource);

       private:
        PostResource(const goldfish_vk::VulkanDispatch&, VkDevice, VkCommandPool,
                     VkFence swapchainImageReleaseFence, VkSemaphore swapchainImageAcquireSemaphore,
                     VkSemaphore swapchainImageReleaseSemaphore, VkCommandBuffer);
        const goldfish_vk::VulkanDispatch& m_vk;
        const VkDevice m_vkDevice;
        const VkCommandPool m_vkCommandPool;
    };

    std::deque<std::shared_ptr<PostResource>> m_freePostResources;
    std::vector<std::optional<std::shared_future<std::shared_ptr<PostResource>>>>
        m_postResourceFutures;
    int m_inFlightFrameIndex;

    class ImageBorrowResource {
       public:
        const VkFence m_completeFence;
        const VkCommandBuffer m_vkCommandBuffer;
        static std::unique_ptr<ImageBorrowResource> create(const goldfish_vk::VulkanDispatch&,
                                                           VkDevice, VkCommandPool);
        ~ImageBorrowResource();
        DISALLOW_COPY_ASSIGN_AND_MOVE(ImageBorrowResource);

       private:
        ImageBorrowResource(const goldfish_vk::VulkanDispatch&, VkDevice, VkCommandPool, VkFence,
                            VkCommandBuffer);
        const goldfish_vk::VulkanDispatch& m_vk;
        const VkDevice m_vkDevice;
        const VkCommandPool m_vkCommandPool;
    };
    std::vector<std::unique_ptr<ImageBorrowResource>> m_imageBorrowResources;

    std::unique_ptr<SwapChainStateVk> m_swapChainStateVk;
    bool m_needToRecreateSwapChain = true;

    std::unordered_map<VkFormat, VkFormatProperties> m_vkFormatProperties;
};
#endif

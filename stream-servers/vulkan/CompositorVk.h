#ifndef COMPOSITOR_VK_H
#define COMPOSITOR_VK_H

#include <array>
#include <deque>
#include <future>
#include <glm/glm.hpp>
#include <list>
#include <memory>
#include <optional>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "BorrowedImage.h"
#include "BorrowedImageVk.h"
#include "Compositor.h"
#include "Hwc2.h"
#include "base/Lock.h"
#include "base/LruCache.h"
#include "vulkan/cereal/common/goldfish_vk_dispatch.h"
#include "vulkan/vk_util.h"

// We do see a composition requests with 12 layers. (b/222700096)
// Inside hwc2, we will ask for surfaceflinger to
// do the composition, if the layers more than 16.
// If we see rendering error or significant time spent on updating
// descriptors in setComposition, we should tune this number.
static constexpr const uint32_t kMaxLayersPerFrame = 16;

// Base used to grant visibility to members to the vk_util::* helper classes.
struct CompositorVkBase : public vk_util::MultiCrtp<CompositorVkBase,         //
                                                    vk_util::FindMemoryType,  //
                                                    vk_util::RunSingleTimeCommand> {
    const goldfish_vk::VulkanDispatch& m_vk;
    const VkDevice m_vkDevice;
    const VkPhysicalDevice m_vkPhysicalDevice;
    const VkQueue m_vkQueue;
    const uint32_t m_queueFamilyIndex;
    std::shared_ptr<android::base::Lock> m_vkQueueLock;
    VkDescriptorSetLayout m_vkDescriptorSetLayout;
    VkPipelineLayout m_vkPipelineLayout;
    VkRenderPass m_vkRenderPass;
    VkPipeline m_graphicsVkPipeline;
    VkBuffer m_vertexVkBuffer;
    VkDeviceMemory m_vertexVkDeviceMemory;
    VkBuffer m_indexVkBuffer;
    VkDeviceMemory m_indexVkDeviceMemory;
    VkDescriptorPool m_vkDescriptorPool;
    VkCommandPool m_vkCommandPool;
    // TODO: create additional VkSampler-s for YCbCr layers.
    VkSampler m_vkSampler;

    // The underlying storage for all of the uniform buffer objects.
    struct UniformBufferStorage {
        VkBuffer m_vkBuffer = VK_NULL_HANDLE;
        VkDeviceMemory m_vkDeviceMemory = VK_NULL_HANDLE;
        VkDeviceSize m_stride = 0;
    } m_uniformStorage;

    // Keep in sync with vulkan/Compositor.frag.
    struct SamplerBinding {
        VkImageView sampledImageView = VK_NULL_HANDLE;
    };

    // Keep in sync with vulkan/Compositor.vert.
    struct UniformBufferBinding {
        alignas(16) glm::mat4 positionTransform;
        alignas(16) glm::mat4 texCoordTransform;
    };

    // The cached contents of a given descriptor set.
    struct DescriptorSetContents {
        SamplerBinding binding0;
        UniformBufferBinding binding1;
    };

    // The cached contents of all descriptors sets of a given frame.
    struct FrameDescriptorSetsContents {
        std::vector<DescriptorSetContents> descriptorSets;
    };

    friend bool operator==(const DescriptorSetContents& lhs, const DescriptorSetContents& rhs);

    friend bool operator==(const FrameDescriptorSetsContents& lhs,
                           const FrameDescriptorSetsContents& rhs);

    struct PerFrameResources {
        VkFence m_vkFence = VK_NULL_HANDLE;
        VkCommandBuffer m_vkCommandBuffer = VK_NULL_HANDLE;
        std::vector<VkDescriptorSet> m_layerDescriptorSets;
        // Pointers into the underlying uniform buffer storage for the uniform
        // buffer of part of each descriptor set for each layer.
        std::vector<UniformBufferBinding*> m_layerUboStorages;
        std::optional<FrameDescriptorSetsContents> m_vkDescriptorSetsContents;
    };
    std::vector<PerFrameResources> m_frameResources;
    std::deque<std::shared_future<PerFrameResources*>> m_availableFrameResources;

    explicit CompositorVkBase(const goldfish_vk::VulkanDispatch& vk, VkDevice device,
                              VkPhysicalDevice physicalDevice, VkQueue queue,
                              std::shared_ptr<android::base::Lock> queueLock,
                              uint32_t queueFamilyIndex, uint32_t maxFramesInFlight)
        : m_vk(vk),
          m_vkDevice(device),
          m_vkPhysicalDevice(physicalDevice),
          m_vkQueue(queue),
          m_queueFamilyIndex(queueFamilyIndex),
          m_vkQueueLock(queueLock),
          m_vkDescriptorSetLayout(VK_NULL_HANDLE),
          m_vkPipelineLayout(VK_NULL_HANDLE),
          m_vkRenderPass(VK_NULL_HANDLE),
          m_graphicsVkPipeline(VK_NULL_HANDLE),
          m_vertexVkBuffer(VK_NULL_HANDLE),
          m_vertexVkDeviceMemory(VK_NULL_HANDLE),
          m_indexVkBuffer(VK_NULL_HANDLE),
          m_indexVkDeviceMemory(VK_NULL_HANDLE),
          m_vkDescriptorPool(VK_NULL_HANDLE),
          m_vkCommandPool(VK_NULL_HANDLE),
          m_vkSampler(VK_NULL_HANDLE),
          m_frameResources(maxFramesInFlight) {}
};

class CompositorVk : protected CompositorVkBase, public Compositor {
   public:
    static std::unique_ptr<CompositorVk> create(const goldfish_vk::VulkanDispatch& vk,
                                                VkDevice vkDevice,
                                                VkPhysicalDevice vkPhysicalDevice, VkQueue vkQueue,
                                                std::shared_ptr<android::base::Lock> queueLock,
                                                uint32_t queueFamilyIndex,
                                                uint32_t maxFramesInFlight);

    ~CompositorVk();

    CompositionFinishedWaitable compose(const CompositionRequest& compositionRequest) override;

    void onImageDestroyed(uint32_t imageId) override;

    static bool queueSupportsComposition(const VkQueueFamilyProperties& properties) {
        return properties.queueFlags & VK_QUEUE_GRAPHICS_BIT;
    }

   private:
    explicit CompositorVk(const goldfish_vk::VulkanDispatch&, VkDevice, VkPhysicalDevice, VkQueue,
                          std::shared_ptr<android::base::Lock> queueLock, uint32_t queueFamilyIndex,
                          uint32_t maxFramesInFlight);

    void setUpGraphicsPipeline();
    void setUpVertexBuffers();
    void setUpSampler();
    void setUpDescriptorSets();
    void setUpUniformBuffers();
    void setUpCommandPool();
    void setUpFences();
    void setUpFrameResourceFutures();

    std::optional<std::tuple<VkBuffer, VkDeviceMemory>> createBuffer(VkDeviceSize,
                                                                     VkBufferUsageFlags,
                                                                     VkMemoryPropertyFlags) const;
    std::tuple<VkBuffer, VkDeviceMemory> createStagingBufferWithData(const void* data,
                                                                     VkDeviceSize size) const;
    void copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize) const;

    VkFormatFeatureFlags getFormatFeatures(VkFormat format, VkImageTiling tiling);

    // Check if the ColorBuffer can be used as a compose layer to be sampled from.
    bool canCompositeFrom(const VkImageCreateInfo& info);

    // Check if the ColorBuffer can be used as a render target of a composition.
    bool canCompositeTo(const VkImageCreateInfo& info);

    // A consolidated view of a `Compositor::CompositionRequestLayer` with only
    // the Vulkan components needed for command recording and submission.
    struct CompositionLayerVk {
        VkImage image = VK_NULL_HANDLE;
        VkImageView imageView = VK_NULL_HANDLE;
        VkImageLayout preCompositionLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        uint32_t preCompositionQueueFamilyIndex = 0;
        VkImageLayout postCompositionLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        uint32_t postCompositionQueueFamilyIndex = 0;
    };

    // A consolidated view of a `Compositor::CompositionRequest` with only
    // the Vulkan components needed for command recording and submission.
    struct CompositionVk {
        const BorrowedImageInfoVk* targetImage = nullptr;
        VkFramebuffer targetFramebuffer = VK_NULL_HANDLE;
        std::vector<const BorrowedImageInfoVk*> layersSourceImages;
        FrameDescriptorSetsContents layersDescriptorSets;
    };
    void buildCompositionVk(const CompositionRequest& compositionRequest,
                            CompositionVk* compositionVk);

    void updateDescriptorSetsIfChanged(const FrameDescriptorSetsContents& contents,
                                       PerFrameResources* frameResources);

    class RenderTarget {
       public:
        ~RenderTarget();

        DISALLOW_COPY_ASSIGN_AND_MOVE(RenderTarget);

       private:
        friend class CompositorVk;
        RenderTarget(const goldfish_vk::VulkanDispatch& vk, VkDevice vkDevice, VkImage vkImage,
                     VkImageView vkImageView, uint32_t width, uint32_t height,
                     VkRenderPass vkRenderPass);

        const goldfish_vk::VulkanDispatch& m_vk;
        VkDevice m_vkDevice;
        VkImage m_vkImage;
        VkFramebuffer m_vkFramebuffer;
        uint32_t m_width;
        uint32_t m_height;
    };

    // Gets the RenderTarget used for composing into the given image if it already exists,
    // otherwise creates it.
    RenderTarget* getOrCreateRenderTargetInfo(const BorrowedImageInfoVk& info);

    // Cached format properties used for checking if composition is supported with a given
    // format.
    std::unordered_map<VkFormat, VkFormatProperties> m_vkFormatProperties;

    uint32_t m_maxFramesInFlight = 0;

    static constexpr const VkFormat k_renderTargetFormat = VK_FORMAT_R8G8B8A8_UNORM;
    static constexpr const uint32_t k_renderTargetCacheSize = 128;
    // Maps from borrowed image ids to render target info.
    android::base::LruCache<uint32_t, std::unique_ptr<RenderTarget>> m_renderTargetCache;
};

#endif /* COMPOSITOR_VK_H */

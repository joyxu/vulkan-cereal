#ifndef VK_TEST_UTILS_H
#define VK_TEST_UTILS_H

#include "vulkan/VulkanDispatch.h"
#include "vulkan/vk_util.h"

namespace emugl {

struct RenderResourceVkBase
    : public vk_util::FindMemoryType<
          RenderResourceVkBase,
          vk_util::RunSingleTimeCommand<
              RenderResourceVkBase, vk_util::RecordImageLayoutTransformCommands<
                                        RenderResourceVkBase>>> {
    const goldfish_vk::VulkanDispatch &m_vk;
    VkDevice m_vkDevice;
    VkPhysicalDevice m_vkPhysicalDevice;
    VkQueue m_vkQueue;
    uint32_t m_width;
    uint32_t m_height;

    VkImage m_vkImage;
    VkDeviceMemory m_imageVkDeviceMemory;
    VkImageView m_vkImageView;

    VkBuffer m_vkBuffer;
    VkDeviceMemory m_bufferVkDeviceMemory;
    uint32_t *m_memory;

    VkCommandPool m_vkCommandPool;
    VkCommandBuffer m_readCommandBuffer;
    VkCommandBuffer m_writeCommandBuffer;

    explicit RenderResourceVkBase(const goldfish_vk::VulkanDispatch &vk)
        : m_vk(vk),
          m_vkImage(VK_NULL_HANDLE),
          m_imageVkDeviceMemory(VK_NULL_HANDLE),
          m_vkImageView(VK_NULL_HANDLE),
          m_vkBuffer(VK_NULL_HANDLE),
          m_bufferVkDeviceMemory(VK_NULL_HANDLE),
          m_memory(nullptr),
          m_readCommandBuffer(VK_NULL_HANDLE),
          m_writeCommandBuffer(VK_NULL_HANDLE) {}
};

template <VkImageLayout imageLayout, VkImageUsageFlags imageUsage>
struct RenderResourceVk : public RenderResourceVkBase {
   public:
    static constexpr VkFormat k_vkFormat = VK_FORMAT_R8G8B8A8_SRGB;
    static constexpr uint32_t k_bpp = 4;
    static constexpr VkImageLayout k_vkImageLayout = imageLayout;

    static std::unique_ptr<const RenderResourceVk<imageLayout, imageUsage>>
    create(const goldfish_vk::VulkanDispatch &vk, VkDevice device,
           VkPhysicalDevice physicalDevice, VkQueue queue,
           VkCommandPool commandPool, uint32_t width, uint32_t height) {
        std::unique_ptr<RenderResourceVk<imageLayout, imageUsage>> res(
            new RenderResourceVk<imageLayout, imageUsage>(vk));
        res->m_vkDevice = device;
        res->m_vkPhysicalDevice = physicalDevice;
        res->m_vkQueue = queue;
        res->m_width = width;
        res->m_height = height;
        res->m_vkCommandPool = commandPool;
        if (!res->setUpImage()) {
            return nullptr;
        }
        if (!res->setUpBuffer()) {
            return nullptr;
        }
        if (!res->setUpCommandBuffer()) {
            return nullptr;
        }

        return res;
    }

    uint32_t numOfPixels() const { return m_width * m_height; }

    bool write(const std::vector<uint32_t> &pixels) const {
        if (pixels.size() != numOfPixels()) {
            return false;
        }
        std::copy(pixels.begin(), pixels.end(), m_memory);
        return submitCommandBufferAndWait(m_writeCommandBuffer);
    }

    std::optional<std::vector<uint32_t>> read() const {
        std::vector<uint32_t> res(numOfPixels());
        if (!submitCommandBufferAndWait(m_readCommandBuffer)) {
            return std::nullopt;
        }
        std::copy(m_memory, m_memory + numOfPixels(), res.begin());
        return res;
    }

    ~RenderResourceVk() {
        std::vector<VkCommandBuffer> toFree;
        if (m_writeCommandBuffer != VK_NULL_HANDLE) {
            toFree.push_back(m_writeCommandBuffer);
        }
        if (m_readCommandBuffer != VK_NULL_HANDLE) {
            toFree.push_back(m_readCommandBuffer);
        }
        if (!toFree.empty()) {
            m_vk.vkFreeCommandBuffers(m_vkDevice, m_vkCommandPool,
                                      static_cast<uint32_t>(toFree.size()),
                                      toFree.data());
        }
        if (m_memory) {
            m_vk.vkUnmapMemory(m_vkDevice, m_bufferVkDeviceMemory);
        }
        if (m_bufferVkDeviceMemory != VK_NULL_HANDLE) {
            m_vk.vkFreeMemory(m_vkDevice, m_bufferVkDeviceMemory, nullptr);
        }
        if (m_vkBuffer != VK_NULL_HANDLE) {
            m_vk.vkDestroyBuffer(m_vkDevice, m_vkBuffer, nullptr);
        }
        if (m_vkImageView != VK_NULL_HANDLE) {
            m_vk.vkDestroyImageView(m_vkDevice, m_vkImageView, nullptr);
        }
        if (m_imageVkDeviceMemory != VK_NULL_HANDLE) {
            m_vk.vkFreeMemory(m_vkDevice, m_imageVkDeviceMemory, nullptr);
        }
        if (m_vkImage != VK_NULL_HANDLE) {
            m_vk.vkDestroyImage(m_vkDevice, m_vkImage, nullptr);
        }
    }

   private:
    RenderResourceVk(const goldfish_vk::VulkanDispatch &vk)
        : RenderResourceVkBase(vk) {}

    bool setUpImage() {
        VkImageCreateInfo imageCi{};
        imageCi.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCi.imageType = VK_IMAGE_TYPE_2D;
        imageCi.extent.width = m_width;
        imageCi.extent.height = m_height;
        imageCi.extent.depth = 1;
        imageCi.mipLevels = 1;
        imageCi.arrayLayers = 1;
        imageCi.format = k_vkFormat;
        imageCi.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCi.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCi.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | imageUsage;
        imageCi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCi.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCi.flags = 0;
        if (m_vk.vkCreateImage(m_vkDevice, &imageCi, nullptr, &m_vkImage) !=
            VK_SUCCESS) {
            m_vkImage = VK_NULL_HANDLE;
            return false;
        }

        VkMemoryRequirements memRequirements;
        m_vk.vkGetImageMemoryRequirements(m_vkDevice, m_vkImage,
                                          &memRequirements);
        auto maybeMemoryTypeIndex =
            findMemoryType(memRequirements.memoryTypeBits,
                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (!maybeMemoryTypeIndex.has_value()) {
            return false;
        }
        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = maybeMemoryTypeIndex.value();
        if (m_vk.vkAllocateMemory(m_vkDevice, &allocInfo, nullptr,
                                  &m_imageVkDeviceMemory) != VK_SUCCESS) {
            m_imageVkDeviceMemory = VK_NULL_HANDLE;
            return false;
        }
        if (m_vk.vkBindImageMemory(m_vkDevice, m_vkImage, m_imageVkDeviceMemory,
                                   0) != VK_SUCCESS) {
            return false;
        }

        runSingleTimeCommands(m_vkQueue, [this](const auto &cmdBuff) {
            recordImageLayoutTransformCommands(
                cmdBuff, m_vkImage, VK_IMAGE_LAYOUT_UNDEFINED, k_vkImageLayout);
        });

        VkImageViewCreateInfo imageViewCi = {};
        imageViewCi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCi.image = m_vkImage;
        imageViewCi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCi.format = k_vkFormat;
        imageViewCi.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCi.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCi.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCi.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewCi.subresourceRange.baseMipLevel = 0;
        imageViewCi.subresourceRange.levelCount = 1;
        imageViewCi.subresourceRange.baseArrayLayer = 0;
        imageViewCi.subresourceRange.layerCount = 1;
        if (m_vk.vkCreateImageView(m_vkDevice, &imageViewCi, nullptr,
                                   &m_vkImageView) != VK_SUCCESS) {
            return false;
        }
        return true;
    }

    bool submitCommandBufferAndWait(VkCommandBuffer cmdBuff) const {
        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdBuff;
        if (m_vk.vkQueueSubmit(m_vkQueue, 1, &submitInfo, VK_NULL_HANDLE) !=
            VK_SUCCESS) {
            return false;
        }
        if (m_vk.vkQueueWaitIdle(m_vkQueue) != VK_SUCCESS) {
            return false;
        }
        return true;
    }

    bool setUpBuffer() {
        VkBufferCreateInfo bufferCi = {};
        bufferCi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCi.size = m_width * m_height * k_bpp;
        bufferCi.usage =
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferCi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (m_vk.vkCreateBuffer(m_vkDevice, &bufferCi, nullptr, &m_vkBuffer) !=
            VK_SUCCESS) {
            m_vkBuffer = VK_NULL_HANDLE;
            return false;
        }

        VkMemoryRequirements memRequirements;
        m_vk.vkGetBufferMemoryRequirements(m_vkDevice, m_vkBuffer,
                                           &memRequirements);
        auto maybeMemoryTypeIndex =
            findMemoryType(memRequirements.memoryTypeBits,
                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        if (!maybeMemoryTypeIndex.has_value()) {
            return false;
        }
        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = maybeMemoryTypeIndex.value();
        if (m_vk.vkAllocateMemory(m_vkDevice, &allocInfo, nullptr,
                                  &m_bufferVkDeviceMemory) != VK_SUCCESS) {
            m_bufferVkDeviceMemory = VK_NULL_HANDLE;
            return false;
        }
        if (m_vk.vkBindBufferMemory(m_vkDevice, m_vkBuffer,
                                    m_bufferVkDeviceMemory, 0) != VK_SUCCESS) {
            return false;
        }
        if (m_vk.vkMapMemory(
                m_vkDevice, m_bufferVkDeviceMemory, 0, bufferCi.size, 0,
                reinterpret_cast<void **>(&m_memory)) != VK_SUCCESS) {
            m_memory = nullptr;
            return false;
        }
        return true;
    }

    bool setUpCommandBuffer() {
        std::array<VkCommandBuffer, 2> cmdBuffs;
        VkCommandBufferAllocateInfo cmdBuffAllocInfo = {};
        cmdBuffAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdBuffAllocInfo.commandPool = m_vkCommandPool;
        cmdBuffAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdBuffAllocInfo.commandBufferCount =
            static_cast<uint32_t>(cmdBuffs.size());
        if (m_vk.vkAllocateCommandBuffers(m_vkDevice, &cmdBuffAllocInfo,
                                          cmdBuffs.data()) != VK_SUCCESS) {
            return false;
        }
        m_readCommandBuffer = cmdBuffs[0];
        m_writeCommandBuffer = cmdBuffs[1];

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        if (m_vk.vkBeginCommandBuffer(m_readCommandBuffer, &beginInfo) !=
            VK_SUCCESS) {
            return false;
        }
        recordImageLayoutTransformCommands(
            m_readCommandBuffer, m_vkImage, k_vkImageLayout,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        VkBufferImageCopy region = {};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {m_width, m_height, 1};
        m_vk.vkCmdCopyImageToBuffer(m_readCommandBuffer, m_vkImage,
                                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                    m_vkBuffer, 1, &region);
        recordImageLayoutTransformCommands(m_readCommandBuffer, m_vkImage,
                                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                           k_vkImageLayout);
        if (m_vk.vkEndCommandBuffer(m_readCommandBuffer) != VK_SUCCESS) {
            return false;
        }

        if (m_vk.vkBeginCommandBuffer(m_writeCommandBuffer, &beginInfo) !=
            VK_SUCCESS) {
            return false;
        }
        recordImageLayoutTransformCommands(
            m_writeCommandBuffer, m_vkImage, k_vkImageLayout,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        m_vk.vkCmdCopyBufferToImage(m_writeCommandBuffer, m_vkBuffer, m_vkImage,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                                    &region);
        recordImageLayoutTransformCommands(m_writeCommandBuffer, m_vkImage,
                                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                           k_vkImageLayout);
        if (m_vk.vkEndCommandBuffer(m_writeCommandBuffer) != VK_SUCCESS) {
            return false;
        }
        return true;
    }
};

}  // namespace emugl

#endif
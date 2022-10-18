// Copyright 2022 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "stream-servers/vulkan/emulated_textures/AstcTexture.h"
#include "vulkan/cereal/common/goldfish_vk_dispatch.h"
#include "vulkan/vulkan.h"

namespace goldfish_vk {

struct CompressedImageInfo {
    bool isCompressed = false;
    bool isEtc2 = false;
    bool isAstc = false;
    VkDevice device = 0;
    VkFormat compFormat;  // The compressed format
    VkImageType imageType;
    VkImageCreateInfo sizeCompImgCreateInfo;
    std::vector<uint32_t> sizeCompImgQueueFamilyIndices;
    VkFormat sizeCompFormat;  // Size compatible format
    VkDeviceSize alignment = 0;
    std::vector<VkDeviceSize> memoryOffsets = {};
    std::vector<VkImage> sizeCompImgs;                 // Size compatible images
    VkFormat decompFormat = VK_FORMAT_R8G8B8A8_UNORM;  // Decompressed format
    VkImage decompImg = 0;                             // Decompressed image
    VkExtent3D extent;
    uint32_t compressedBlockWidth = 1;
    uint32_t compressedBlockHeight = 1;
    uint32_t layerCount;
    uint32_t mipLevels = 1;
    std::unique_ptr<AstcTexture> astcTexture = nullptr;
    VkDescriptorSetLayout decompDescriptorSetLayout = 0;
    VkDescriptorPool decompDescriptorPool = 0;
    std::vector<VkDescriptorSet> decompDescriptorSets = {};
    VkShaderModule decompShader = 0;
    VkPipelineLayout decompPipelineLayout = 0;
    VkPipeline decompPipeline = 0;
    std::vector<VkImageView> sizeCompImageViews = {};
    std::vector<VkImageView> decompImageViews = {};

    static CompressedImageInfo create(VkFormat compFmt);

    static VkFormat getDecompFormat(VkFormat compFmt);
    static VkFormat getSizeCompFormat(VkFormat compFmt);

    uint32_t mipmapWidth(uint32_t level) const;
    uint32_t mipmapHeight(uint32_t level) const;
    uint32_t mipmapDepth(uint32_t level) const;
    uint32_t sizeCompMipmapWidth(uint32_t level) const;
    uint32_t sizeCompMipmapHeight(uint32_t level) const;
    uint32_t sizeCompMipmapDepth(uint32_t level) const;
    bool needEmulatedAlpha();

    void createSizeCompImages(goldfish_vk::VulkanDispatch* vk);

    VkResult initDecomp(goldfish_vk::VulkanDispatch* vk, VkDevice device, VkImage image);

    void cmdDecompress(goldfish_vk::VulkanDispatch* vk, VkCommandBuffer commandBuffer,
                       VkPipelineStageFlags dstStageMask, VkImageLayout newLayout,
                       VkAccessFlags dstAccessMask, uint32_t baseMipLevel, uint32_t levelCount,
                       uint32_t baseLayer, uint32_t _layerCount);
};

}  // namespace goldfish_vk

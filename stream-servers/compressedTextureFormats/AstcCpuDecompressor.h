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

#include <memory>

#include "vulkan/cereal/common/goldfish_vk_dispatch.h"
#include "vulkan/vulkan.h"

namespace goldfish_vk {

// This class is responsible for handling ASTC texture decompression on the CPU, if the GPU
// doesn't support ASTC textures.
class AstcCpuDecompressor {
   public:
    virtual ~AstcCpuDecompressor() = default;

    virtual bool initialized() const = 0;  // Whether initialize() was successful.
    virtual bool successful() const = 0;   // Whether we successfully decompressed the image.

    virtual void initialize(VulkanDispatch* vk, VkDevice device, VkPhysicalDevice physicalDevice,
                            VkExtent3D imgSize, uint32_t blockWidth, uint32_t blockHeight) = 0;

    // Releases the resources used by this class.
    // The object can be reused by calling initialize() again.
    virtual void release() = 0;

    virtual void on_vkCmdCopyBufferToImage(VkCommandBuffer commandBuffer, uint8_t* srcAstcData,
                                           size_t astcDataSize, VkImage dstImage,
                                           VkImageLayout dstImageLayout, uint32_t regionCount,
                                           const VkBufferImageCopy* pRegions) = 0;
};

std::unique_ptr<AstcCpuDecompressor> CreateAstcCpuDecompressor();

}  // namespace goldfish_vk

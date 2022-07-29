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

#include "AstcCpuDecompressor.h"

namespace goldfish_vk {

namespace {

class AstcCpuDecompressorNoOp : public AstcCpuDecompressor {
   public:
    bool initialized() const override { return false; }
    bool successful() const override { return false; }

    void initialize(VulkanDispatch* vk, VkDevice device, VkPhysicalDevice physicalDevice,
                    VkExtent3D imgSize, uint32_t blockWidth, uint32_t blockHeight) override {}

    void release() override {}

    void on_vkCmdCopyBufferToImage(VkCommandBuffer commandBuffer, uint8_t* srcAstcData,
                                   size_t astcDataSize, VkImage dstImage,
                                   VkImageLayout dstImageLayout, uint32_t regionCount,
                                   const VkBufferImageCopy* pRegions) override {}
};

}  // namespace

std::unique_ptr<AstcCpuDecompressor> CreateAstcCpuDecompressor() {
    return std::make_unique<AstcCpuDecompressorNoOp>();
}

}  // namespace goldfish_vk
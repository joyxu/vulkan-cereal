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

#include <optional>
#include <vector>

#include "AstcCpuDecompressor.h"
#include "astcenc.h"
#include "host-common/logging.h"
#include "vk_util.h"

namespace goldfish_vk {

namespace {

uint32_t mipmapSize(uint32_t size, uint32_t mipLevel) {
    return std::max<uint32_t>(size >> mipLevel, 1);
}

class AstcCpuDecompressorImpl : public AstcCpuDecompressor {
   public:
    bool initialized() const override { return mVk && mDecoderContext; }

    void initialize(VulkanDispatch* vk, VkDevice device, VkPhysicalDevice physicalDevice,
                    VkExtent3D imgSize, uint32_t blockWidth, uint32_t blockHeight) override {
        mSuccess = false;
        mVk = vk;
        mDevice = device;
        mPhysicalDevice = physicalDevice;
        mImgSize = imgSize;
        mBlockWidth = blockWidth;
        mBlockHeight = blockHeight;

        astcenc_error status;
        astcenc_config config = {};
        status =
            // TODO(gregschlom): Do we need to pass ASTCENC_PRF_LDR_SRGB here?
            astcenc_config_init(ASTCENC_PRF_LDR, mBlockWidth, mBlockHeight, 1, ASTCENC_PRE_FASTEST,
                                ASTCENC_FLG_DECOMPRESS_ONLY, &config);
        if (status != ASTCENC_SUCCESS) {
            WARN("ASTC decoder: astcenc_config_init() failed: %s",
                 astcenc_get_error_string(status));
            return;
        }

        status = astcenc_context_alloc(&config, /*thread_count=*/1, &mDecoderContext);
        if (status != ASTCENC_SUCCESS) {
            WARN("ASTC decoder: astcenc_context_alloc() failed: %s",
                 astcenc_get_error_string(status));
        }
    }

    void release() override {
        mSuccess = false;
        astcenc_context_free(mDecoderContext);
        mDecoderContext = nullptr;
        if (mVk && mDevice) {
            mVk->vkDestroyBuffer(mDevice, mDecompBuffer, nullptr);
            mVk->vkFreeMemory(mDevice, mDecompBufferMemory, nullptr);
            mDecompBuffer = VK_NULL_HANDLE;
            mDecompBufferMemory = VK_NULL_HANDLE;
        }
    }

    void on_vkCmdCopyBufferToImage(VkCommandBuffer commandBuffer, uint8_t* srcAstcData,
                                   size_t astcDataSize, VkImage dstImage,
                                   VkImageLayout dstImageLayout, uint32_t regionCount,
                                   const VkBufferImageCopy* pRegions) override {
        mSuccess = false;
        VkResult res;

        if (!initialized()) return;

        size_t decompSize = 0;  // How many bytes we need to hold the decompressed data

        // Make a copy of the regions and update the buffer offset of each to reflect the
        // correct location of the decompressed data
        std::vector<VkBufferImageCopy> decompRegions(pRegions, pRegions + regionCount);
        for (auto& decompRegion : decompRegions) {
            const uint32_t mipLevel = decompRegion.imageSubresource.mipLevel;
            const uint32_t width = mipmapSize(mImgSize.width, mipLevel);
            const uint32_t height = mipmapSize(mImgSize.height, mipLevel);

            // TODO(gregschlom) deal with those cases. See details at:
            // https://registry.khronos.org/vulkan/specs/1.0-extensions/html/chap20.html#copies-buffers-images-addressing
            // https://stackoverflow.com/questions/46501832/vulkan-vkbufferimagecopy-for-partial-transfer
            if (decompRegion.bufferRowLength != 0 || decompRegion.bufferImageHeight != 0) {
                WARN("ASTC CPU decompression skipped: non-packed buffer");
                return;
            }
            if (decompRegion.imageOffset.x != 0 || decompRegion.imageOffset.y != 0) {
                WARN("ASTC CPU decompression skipped: imageOffset is non-zero");
                return;
            }
            if (decompRegion.imageExtent.width != width ||
                decompRegion.imageExtent.height != height) {
                WARN("ASTC CPU decompression skipped: imageExtent is less than the entire image");
                return;
            }

            decompRegion.bufferOffset = decompSize;
            decompSize += width * height * 4;
        }

        std::vector<uint8_t> decompData(decompSize);

        // Decompress each region
        for (int i = 0; i < regionCount; i++) {
            const auto& compRegion = pRegions[i];
            const auto& decompRegion = decompRegions[i];
            const uint32_t mipLevel = decompRegion.imageSubresource.mipLevel;
            const uint32_t width = mipmapSize(mImgSize.width, mipLevel);
            const uint32_t height = mipmapSize(mImgSize.height, mipLevel);

            const size_t numBlocks = ((width + mBlockWidth - 1) / mBlockWidth) *
                                     ((height + mBlockHeight - 1) / mBlockHeight);

            const size_t compressedSize = numBlocks * 16;
            if (compRegion.bufferOffset + compressedSize > astcDataSize) {
                WARN(
                    "ASTC CPU decompression failed: compressed data size (%llu) is larger than the "
                    "buffer (%llu)",
                    compRegion.bufferOffset + compressedSize, astcDataSize);
                return;
            }

            uint8_t* decompLocation = decompData.data() + decompRegion.bufferOffset;
            astcenc_image image = {
                .dim_x = width,
                .dim_y = height,
                .dim_z = 1,
                .data_type = ASTCENC_TYPE_U8,
                .data = reinterpret_cast<void**>(&decompLocation),
            };
            const astcenc_swizzle swizzle = {ASTCENC_SWZ_R, ASTCENC_SWZ_G, ASTCENC_SWZ_B,
                                             ASTCENC_SWZ_A};
            astcenc_error status =
                astcenc_decompress_image(mDecoderContext, srcAstcData + compRegion.bufferOffset,
                                         compressedSize, &image, &swizzle, 0);
            if (status != ASTCENC_SUCCESS) {
                WARN("ASTC CPU decompression failed: %s - will try compute shader instead.",
                     astcenc_get_error_string(status));
                return;
            }
        }

        if (mDecompBuffer || mDecompBufferMemory) {
            WARN("ASTC CPU decompression failed: tried to decompress same image more than once");
            return;
        }

        // Copy the decompressed data to a new VkBuffer
        VkBufferCreateInfo bufferInfo = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = decompData.size(),
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        res = mVk->vkCreateBuffer(mDevice, &bufferInfo, nullptr, &mDecompBuffer);
        if (res != VK_SUCCESS) {
            WARN("ASTC CPU decompression: vkCreateBuffer failed: %d", res);
            mDecompBuffer = VK_NULL_HANDLE;
            return;
        }

        VkMemoryRequirements memRequirements;
        mVk->vkGetBufferMemoryRequirements(mDevice, mDecompBuffer, &memRequirements);

        std::optional<uint32_t> memIndex = vk_util::findMemoryType(
            mVk, mPhysicalDevice, memRequirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (!memIndex) {
            WARN("ASTC CPU decompression: findProperties failed");
            return;
        }

        VkMemoryAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = memRequirements.size,
            .memoryTypeIndex = *memIndex,
        };
        res = mVk->vkAllocateMemory(mDevice, &allocInfo, nullptr, &mDecompBufferMemory);
        if (res != VK_SUCCESS) {
            WARN("ASTC CPU decompression: vkAllocateMemory failed: %d", res);
            mDecompBufferMemory = VK_NULL_HANDLE;
            return;
        }

        res = mVk->vkBindBufferMemory(mDevice, mDecompBuffer, mDecompBufferMemory, 0);
        if (res != VK_SUCCESS) {
            WARN("ASTC CPU decompression: vkBindBufferMemory failed: %d", res);
            return;
        }

        // Copy data to buffer
        void* data;
        res = mVk->vkMapMemory(mDevice, mDecompBufferMemory, 0, decompData.size(), 0, &data);
        if (res != VK_SUCCESS) {
            WARN("ASTC CPU decompression: vkMapMemory failed: %d", res);
            return;
        }
        memcpy(data, decompData.data(), decompData.size());
        mVk->vkUnmapMemory(mDevice, mDecompBufferMemory);

        // Finally, actually copy the buffer to the image
        mVk->vkCmdCopyBufferToImage(commandBuffer, mDecompBuffer, dstImage, dstImageLayout,
                                    decompRegions.size(), decompRegions.data());

        mSuccess = true;
    }

    bool successful() const override { return mSuccess; }

   private:
    bool mSuccess = false;  // true if the image was successfully decompressed
    VulkanDispatch* mVk = nullptr;
    VkDevice mDevice = VK_NULL_HANDLE;
    VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
    VkExtent3D mImgSize = {};
    uint32_t mBlockWidth = 0;
    uint32_t mBlockHeight = 0;
    astcenc_context* mDecoderContext = nullptr;
    VkBuffer mDecompBuffer = VK_NULL_HANDLE;              // VkBuffer of the decompressed image
    VkDeviceMemory mDecompBufferMemory = VK_NULL_HANDLE;  // memory of the decompressed image
};

}  // namespace

std::unique_ptr<AstcCpuDecompressor> CreateAstcCpuDecompressor() {
    return std::make_unique<AstcCpuDecompressorImpl>();
}

}  // namespace goldfish_vk
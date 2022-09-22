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

#include "AstcTexture.h"

#include <atomic>
#include <chrono>
#include <optional>
#include <vector>

#include "host-common/logging.h"
#include "stream-servers/vulkan/vk_util.h"

namespace goldfish_vk {
namespace {

using std::chrono::milliseconds;

// Print stats each time we decompress this many pixels:
constexpr uint64_t kProcessedPixelsLogInterval = 10'000'000;

std::atomic<uint64_t> pixels_processed = 0;
std::atomic<uint64_t> ms_elapsed = 0;

uint32_t mipmapSize(uint32_t size, uint32_t mipLevel) {
    return std::max<uint32_t>(size >> mipLevel, 1);
}

}  // namespace

AstcTexture::AstcTexture(VulkanDispatch* vk, VkDevice device, VkPhysicalDevice physicalDevice,
                         VkExtent3D imgSize, uint32_t blockWidth, uint32_t blockHeight,
                         AstcCpuDecompressor* decompressor)
    : mVk(vk),
      mDevice(device),
      mPhysicalDevice(physicalDevice),
      mImgSize(imgSize),
      mBlockWidth(blockWidth),
      mBlockHeight(blockHeight),
      mDecompressor(decompressor) {}

AstcTexture::~AstcTexture() {
    if (mVk && mDevice) {
        mVk->vkDestroyBuffer(mDevice, mDecompBuffer, nullptr);
        mVk->vkFreeMemory(mDevice, mDecompBufferMemory, nullptr);
    }
}

bool AstcTexture::canDecompressOnCpu() const { return mDecompressor->available(); }

void AstcTexture::on_vkCmdCopyBufferToImage(VkCommandBuffer commandBuffer, uint8_t* srcAstcData,
                                            size_t astcDataSize, VkImage dstImage,
                                            VkImageLayout dstImageLayout, uint32_t regionCount,
                                            const VkBufferImageCopy* pRegions) {
    auto start_time = std::chrono::steady_clock::now();
    mSuccess = false;
    VkResult res;

    if (mDecompBuffer || mDecompBufferMemory) {
        WARN(
            "ASTC CPU decompression failed: tried to decompress same image more than once. Falling"
            " back to GPU decompression");
        return;
    }

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
        if (decompRegion.imageExtent.width != width || decompRegion.imageExtent.height != height) {
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

        int32_t status = mDecompressor->decompress(
            width, height, mBlockWidth, mBlockHeight, srcAstcData + compRegion.bufferOffset,
            compressedSize, decompData.data() + decompRegion.bufferOffset);

        if (status != 0) {
            WARN("ASTC CPU decompression failed: %s - will try compute shader instead.",
                 mDecompressor->getStatusString(status));
            return;
        }
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
    auto end_time = std::chrono::steady_clock::now();

    // Compute stats
    pixels_processed += decompSize / 4;
    ms_elapsed += std::chrono::duration_cast<milliseconds>(end_time - start_time).count();

    uint64_t total_pixels = pixels_processed.load();
    uint64_t total_time = ms_elapsed.load();

    if (total_pixels >= kProcessedPixelsLogInterval && total_time > 0) {
        pixels_processed.store(0);
        ms_elapsed.store(0);
        INFO("ASTC CPU decompression: %.2f Mpix in %.2f seconds (%.2f Mpix/s)",
             total_pixels / 1'000'000.0, total_time / 1000.0,
             (float)total_pixels / total_time / 1000.0);
    }
}

}  // namespace goldfish_vk
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

// Used by std::unique_ptr to release the context when the pointer is destroyed
struct AstcencContextDeleter {
    void operator()(astcenc_context* c) { astcenc_context_free(c); }
};

using AstcencContextUniquePtr = std::unique_ptr<astcenc_context, AstcencContextDeleter>;

uint32_t mipmapSize(uint32_t size, uint32_t mipLevel) {
    return std::max<uint32_t>(size >> mipLevel, 1);
}

// Caches and manages astcenc_context objects.
//
// Each context is fairly large (around 30 MB) and takes a while to construct, so it's important to
// reuse them as much as possible.
//
// While context objects can be reused across multiple threads, they must be used sequentially. To
// avoid having to lock and manage access between threads, we keep one cache per thread. This avoids
// any concurrency issues, at the cost of extra memory.
//
// Currently, there is no eviction strategy. Each cache could grow to a maximum of ~400 MB in size
// since they are 13 possible ASTC block sizes.
class AstcDecoderContextCache {
   public:
    // Returns the singleton instance of this class.
    // The singleton is thread-local: each thread gets its own instance. Having a separate cache for
    // each thread avoids needing to synchronize access to the context objects.
    static AstcDecoderContextCache& instance() {
        static thread_local AstcDecoderContextCache instance;
        return instance;
    }

    // Returns a context object for a given ASTC block size.
    // Returns null if context initialization failed for any reason.
    astcenc_context* get(uint32_t blockWidth, uint32_t blockHeight) {
        AstcencContextUniquePtr& context = mContexts[{blockWidth, blockHeight}];
        if (context == nullptr) {
            context = makeDecoderContext(blockWidth, blockHeight);
        }
        return context.get();
    }

   private:
    AstcDecoderContextCache() = default;

    // Holds the data we use as the cache key
    struct Key {
        uint32_t blockWidth;
        uint32_t blockHeight;

        bool operator==(const Key& other) const {
            return blockWidth == other.blockWidth && blockHeight == other.blockHeight;
        }
    };

    // Computes the hash of a Key
    struct KeyHash {
        std::size_t operator()(const Key& k) const {
            // blockWidth and blockHeight are < 256 (actually, < 16), so this is safe
            return k.blockWidth << 8 | k.blockHeight;
        }
    };

    // Creates a new astcenc_context and wraps it in a smart pointer, so that we don't need to
    // manually call astcenc_context_free.
    // Returns null if `astcenc_context_alloc` fails.
    AstcencContextUniquePtr makeDecoderContext(uint32_t blockWidth, uint32_t blockHeight) const {
        astcenc_config config = {};
        astcenc_error status =
            // TODO(gregschlom): Do we need to pass ASTCENC_PRF_LDR_SRGB here?
            astcenc_config_init(ASTCENC_PRF_LDR, blockWidth, blockHeight, 1, ASTCENC_PRE_FASTEST,
                                ASTCENC_FLG_DECOMPRESS_ONLY, &config);
        if (status != ASTCENC_SUCCESS) {
            WARN("ASTC decoder: astcenc_config_init() failed: %s",
                 astcenc_get_error_string(status));
            return nullptr;
        }

        astcenc_context* context;
        status = astcenc_context_alloc(&config, /*thread_count=*/1, &context);
        if (status != ASTCENC_SUCCESS) {
            WARN("ASTC decoder: astcenc_context_alloc() failed: %s",
                 astcenc_get_error_string(status));
            return nullptr;
        }
        return AstcencContextUniquePtr(context);
    }

    std::unordered_map<Key, AstcencContextUniquePtr, KeyHash> mContexts;
};

// Performs ASTC decompression of an image on the CPU
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
        mDecoderContext = AstcDecoderContextCache::instance().get(blockWidth, blockHeight);
    }

    void release() override {
        mSuccess = false;
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
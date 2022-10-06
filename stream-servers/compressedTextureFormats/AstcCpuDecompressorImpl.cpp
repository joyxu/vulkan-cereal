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

#include <unordered_map>

#include "AstcCpuDecompressor.h"
#include "astcenc.h"
#include "host-common/logging.h"

namespace goldfish_vk {
namespace {

const astcenc_swizzle kSwizzle = {ASTCENC_SWZ_R, ASTCENC_SWZ_G, ASTCENC_SWZ_B, ASTCENC_SWZ_A};

// Used by std::unique_ptr to release the context when the pointer is destroyed
struct AstcencContextDeleter {
    void operator()(astcenc_context* c) { astcenc_context_free(c); }
};

using AstcencContextUniquePtr = std::unique_ptr<astcenc_context, AstcencContextDeleter>;

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

    // Returns a context object for a given ASTC block size, along with the error code if the
    // context initialization failed.
    // In this case, the context will be null, and the status code will be non-zero.
    std::pair<astcenc_context*, astcenc_error> get(uint32_t blockWidth, uint32_t blockHeight) {
        Value& value = mContexts[{blockWidth, blockHeight}];
        if (value.context == nullptr && value.error == ASTCENC_SUCCESS) {
            value = makeDecoderContext(blockWidth, blockHeight);
        }
        return {value.context.get(), value.error};
    }

   private:
    // Holds the data we use as the cache key
    struct Key {
        uint32_t blockWidth;
        uint32_t blockHeight;

        bool operator==(const Key& other) const {
            return blockWidth == other.blockWidth && blockHeight == other.blockHeight;
        }
    };

    struct Value {
        AstcencContextUniquePtr context = nullptr;
        astcenc_error error = ASTCENC_SUCCESS;
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
    Value makeDecoderContext(uint32_t blockWidth, uint32_t blockHeight) const {
        astcenc_config config = {};
        astcenc_error status =
            // TODO(gregschlom): Do we need to pass ASTCENC_PRF_LDR_SRGB here?
            astcenc_config_init(ASTCENC_PRF_LDR, blockWidth, blockHeight, 1, ASTCENC_PRE_FASTEST,
                                ASTCENC_FLG_DECOMPRESS_ONLY, &config);
        if (status != ASTCENC_SUCCESS) {
            WARN("ASTC decoder: astcenc_config_init() failed: %s",
                 astcenc_get_error_string(status));
            return {nullptr, status};
        }

        astcenc_context* context;
        status = astcenc_context_alloc(&config, /*thread_count=*/1, &context);
        if (status != ASTCENC_SUCCESS) {
            WARN("ASTC decoder: astcenc_context_alloc() failed: %s",
                 astcenc_get_error_string(status));
            return {nullptr, status};
        }
        return {AstcencContextUniquePtr(context), ASTCENC_SUCCESS};
    }

    std::unordered_map<Key, Value, KeyHash> mContexts;
};

// Performs ASTC decompression of an image on the CPU
class AstcCpuDecompressorImpl : public AstcCpuDecompressor {
   public:
    bool available() const override {
        // Try getting an arbitrary context. This checks that we have all the pre-requisites
        // (e.g. the CPU supports AVX2 instructions, etc.)
        auto [context, status] = AstcDecoderContextCache::instance().get(5, 5);
        return status == ASTCENC_SUCCESS;
    }

    int32_t decompress(const uint32_t imgWidth, const uint32_t imgHeight, const uint32_t blockWidth,
                       const uint32_t blockHeight, const uint8_t* astcData, size_t astcDataLength,
                       uint8_t* output) override {
        astcenc_image image = {
            .dim_x = imgWidth,
            .dim_y = imgHeight,
            .dim_z = 1,
            .data_type = ASTCENC_TYPE_U8,
            .data = reinterpret_cast<void**>(&output),
        };

        auto [context, status] = AstcDecoderContextCache::instance().get(blockWidth, blockHeight);
        if (status != ASTCENC_SUCCESS) return status;

        return astcenc_decompress_image(context, astcData, astcDataLength, &image, &kSwizzle, 0);
    }

    const char* getStatusString(int32_t statusCode) const override {
        const char* msg = astcenc_get_error_string((astcenc_error)statusCode);
        return msg ? msg : "ASTCENC_UNKNOWN_STATUS";
    }
};

}  // namespace

AstcCpuDecompressor& AstcCpuDecompressor::get() {
    static AstcCpuDecompressorImpl instance;
    return instance;
}

}  // namespace goldfish_vk
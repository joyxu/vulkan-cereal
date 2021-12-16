// Copyright 2021 The Android Open Source Project
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
#include <ostream>
#include <sstream>
#include <vulkan/vulkan.h>

#include "base/Metrics.h"
#include "host-common/logging.h"

using ::android::base::MetricsLogger;

enum GfxstreamAbortReason : int64_t { VK_RESULT, ABORT_REASON_OTHER = -0x1'0000'0000 };

struct FatalError {
    GfxstreamAbortReason abort_reason;
    VkResult vk_result;

    FatalError(GfxstreamAbortReason ab_reason) : abort_reason(ab_reason) {}
    FatalError(VkResult vk_result) : abort_reason(VK_RESULT), vk_result(vk_result) {}

    inline uint64_t getAbortCode() const {
        return abort_reason == VK_RESULT ? vk_result : abort_reason;
    }
};

class AbortMessage {
   public:
       AbortMessage(const char *file, const char *function, int line, FatalError reason)
        : mFile(file), mFunction(function), mLine(line), mReason(reason) {}

     [[noreturn]] ~AbortMessage() {
           int64_t abortCode = mReason.getAbortCode();
           fprintf(stderr, "FATAL: %s:%d:%s: (error code: %" PRIi64 ")", mFile, mLine, mFunction,
                   abortCode);
           if (!mOss.str().empty()) {
               fprintf(stderr, ": %s", mOss.str().c_str());
           }
           fprintf(stderr, "\n");
           fflush(stderr);
           android::base::CreateMetricsLogger()->logMetricEvent(
               android::base::GfxstreamVkAbort{
                   .file = mFile,
                   .function = mFunction,
                   .msg = mOss.str().c_str(),
                   .line = mLine,
                   .abort_reason = abortCode
                });
           GFXSTREAM_FATAL();
       }

       std::ostream &stream() { return mOss; }
    private:
       const char *mFile, *mFunction;
       int mLine;
       FatalError mReason;
       std::ostringstream mOss;
};

#define GFXSTREAM_ABORT(reason) AbortMessage(__FILE__, __func__, __LINE__, reason).stream()

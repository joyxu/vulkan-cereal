/*
 * Copyright (C) 2011-2021 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef RENDER_DOC_H
#define RENDER_DOC_H

#include <renderdoc_app.h>

#include <cstring>
#include <memory>
#include <mutex>
#include <type_traits>

#include "base/SharedLibrary.h"
#include "host-common/logging.h"
#include "vulkan/vulkan.h"

using android::base::SharedLibrary;

namespace emugl {
class RenderDoc {
   public:
    using RenderDocApi = RENDERDOC_API_1_4_2;
    static std::unique_ptr<RenderDoc> create(const SharedLibrary* renderDocLib) {
        if (!renderDocLib) {
            ERR("The renderdoc shared library is null.");
            return nullptr;
        }
        pRENDERDOC_GetAPI RENDERDOC_GetAPI =
            reinterpret_cast<pRENDERDOC_GetAPI>(renderDocLib->findSymbol("RENDERDOC_GetAPI"));
        if (!RENDERDOC_GetAPI) {
            ERR("Failed to find the RENDERDOC_GetAPI symbol.");
            return nullptr;
        }
        RenderDocApi* rdocApi = nullptr;
        int ret =
            RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_4_2, reinterpret_cast<void**>(&rdocApi));
        if (ret != 1 || rdocApi == nullptr) {
            ERR("Failed to load renderdoc API. %d is returned from RENDERDOC_GetAPI.");
            return nullptr;
        }
        return std::unique_ptr<RenderDoc>(new RenderDoc(rdocApi));
    }

    static constexpr auto kSetActiveWindow = &RenderDocApi::SetActiveWindow;
    static constexpr auto kGetCaptureOptionU32 = &RenderDocApi::GetCaptureOptionU32;
    template <class F, class... Args, typename = std::enable_if_t<std::is_invocable_v<F, Args...>>>
    // Call a RenderDoc in-application API given the function pointer and parameters, and guard the
    // API call with a mutex.
    auto call(F(RenderDocApi::*function), Args... args) {
        std::lock_guard<std::mutex> guard(mMutex);
        return (mRdocApi->*function)(args...);
    }

   private:
    RenderDoc(RenderDocApi* rdocApi) : mRdocApi(rdocApi) {}

    std::mutex mMutex;
    RenderDocApi* mRdocApi = nullptr;
};
}  // namespace emugl

#endif

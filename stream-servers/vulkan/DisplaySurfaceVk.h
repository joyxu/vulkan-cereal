// Copyright (C) 2022 The Android Open Source Project
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

#include "DisplaySurface.h"
#include "render-utils/render_api_platform_types.h"
#include "vulkan/cereal/common/goldfish_vk_dispatch.h"

class DisplaySurfaceVk : public gfxstream::DisplaySurfaceImpl {
  public:
    static std::unique_ptr<DisplaySurfaceVk> create(
        const goldfish_vk::VulkanDispatch& vk,
        VkInstance vkInstance,
        FBNativeWindowType window);

    ~DisplaySurfaceVk();

    VkSurfaceKHR getSurface() const { return mSurface; }

  private:
    DisplaySurfaceVk(const goldfish_vk::VulkanDispatch& vk,
                     VkInstance vkInstance,
                     VkSurfaceKHR vkSurface);

    const goldfish_vk::VulkanDispatch& mVk;
    VkInstance mInstance = VK_NULL_HANDLE;
    VkSurfaceKHR mSurface = VK_NULL_HANDLE;
};

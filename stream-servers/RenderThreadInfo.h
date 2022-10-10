/*
* Copyright (C) 2011 The Android Open Source Project
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
#ifndef _LIB_OPENGL_RENDER_THREAD_INFO_H
#define _LIB_OPENGL_RENDER_THREAD_INFO_H

#include <functional>
#include <memory>
#include <unordered_set>

#include "aemu/base/files/Stream.h"
#include "renderControl_dec/renderControl_dec.h"
#include "RenderThreadInfoGl.h"
#include "RenderThreadInfoVk.h"

// A class used to model the state of each RenderThread related
struct RenderThreadInfo {
    // Create new instance. Only call this once per thread.
    // Future callls to get() will return this instance until
    // it is destroyed.
    RenderThreadInfo();

    // Destructor.
    ~RenderThreadInfo();

    // Return the current thread's instance, if any, or NULL.
    static RenderThreadInfo* get();

    // Loop over all active render thread infos
    static void forAllRenderThreadInfos(std::function<void(RenderThreadInfo*)>);

    void initGl();

    renderControl_decoder_context_t m_rcDec;

    // The unique id of owner guest process of this render thread
    uint64_t                        m_puid = 0;
    std::optional<std::string>      m_processName;

    std::optional<RenderThreadInfoGl> m_glInfo;
    std::optional<goldfish_vk::RenderThreadInfoVk> m_vkInfo;

    // Functions to save / load a snapshot
    // They must be called after Framebuffer snapshot
    void onSave(android::base::Stream* stream);
    bool onLoad(android::base::Stream* stream);

    // Sometimes we can load render thread info before
    // FrameBuffer repopulates the contexts.
    void postLoadRefreshCurrentContextSurfacePtrs();
};

#endif

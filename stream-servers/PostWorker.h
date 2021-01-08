/*
* Copyright (C) 2017 The Android Open Source Project
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
#pragma once

#include "base/Compiler.h"
#include "base/MessageChannel.h"
#include "host-common/window_agent.h"

#include <EGL/egl.h>
#include <GLES/gl.h>
#include <GLES3/gl3.h>

#include <functional>
#include <vector>

#include "Hwc2.h"
#include "base/Compiler.h"
#include "base/Lock.h"

class ColorBuffer;
class FrameBuffer;
struct RenderThreadInfo;

class PostWorker {
public:
    using BindSubwinCallback = std::function<bool(void)>;

    PostWorker(BindSubwinCallback&& cb,
               bool mainThreadPostingOnly,
               EGLContext eglContext,
               EGLSurface eglSurface);
    ~PostWorker();

    // post: posts the next color buffer.
    // Assumes framebuffer lock is held.
    void post(ColorBuffer* cb);

    // viewport: (re)initializes viewport dimensions.
    // Assumes framebuffer lock is held.
    // This is called whenever the subwindow needs a refresh
    // (FrameBuffer::setupSubWindow).
    void viewport(int width, int height);

    // compose: compse the layers into final framebuffer
    void compose(ComposeDevice* p);

    // compose: compse the layers into final framebuffer, version 2
    void compose(ComposeDevice_v2* p);

    // clear: blanks out emulator display when refreshing the subwindow
    // if there is no last posted color buffer to show yet.
    void clear();

    void screenshot(ColorBuffer* cb,
                    int screenwidth,
                    int screenheight,
                    GLenum format,
                    GLenum type,
                    int skinRotation,
                    void* pixels);

private:
    // Impl versions of the above, so we can run it from separate threads
    void postImpl(ColorBuffer* cb);
    void viewportImpl(int width, int height);
    void composeImpl(ComposeDevice* p);
    void composev2Impl(ComposeDevice_v2* p);
    void clearImpl();

    // Subwindow binding
    void bind();
    void unbind();

    void composeLayer(ComposeLayer* l);
    void fillMultiDisplayPostStruct(ComposeLayer* l,
                                    hwc_rect_t displayArea,
                                    hwc_frect_t cropArea,
                                    hwc_transform_t transform);

private:
    using UiThreadRunner = std::function<void(UiUpdateFunc, void*, bool)>;
    struct PostArgs {
        ColorBuffer* postCb;
        int width;
        int height;
        ComposeDevice* composeDevice;
        ComposeDevice_v2* composeDevice_v2;
    };

    RenderThreadInfo* mTLS;
    FrameBuffer* mFb;

    std::function<bool(void)> mBindSubwin;

    bool m_initialized = false;
    int m_viewportWidth = 0;
    int m_viewportHeight = 0;
    GLuint m_composeFbo = 0;

    bool m_mainThreadPostingOnly = false;
    UiThreadRunner m_runOnUiThread = 0;
    android::base::MessageChannel<PostArgs, 1> m_toUiThread;
    android::base::MessageChannel<int, 1> m_fromUiThread;
    EGLContext mContext = EGL_NO_CONTEXT;
    EGLSurface mSurface = EGL_NO_SURFACE;

    DISALLOW_COPY_AND_ASSIGN(PostWorker);
};

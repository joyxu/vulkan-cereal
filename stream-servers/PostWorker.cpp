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
#include "PostWorker.h"

#include <string.h>

#include <chrono>

#include "ColorBuffer.h"
#include "CompositorGl.h"
#include "Debug.h"
#include "FrameBuffer.h"
#include "OpenGLESDispatch/DispatchTables.h"
#include "OpenGLESDispatch/EGLDispatch.h"
#include "OpenGLESDispatch/GLESv2Dispatch.h"
#include "RenderThreadInfo.h"
#include "aemu/base/Tracing.h"
#include "host-common/GfxstreamFatalError.h"
#include "host-common/logging.h"
#include "host-common/misc.h"
#include "vulkan/VkCommonOperations.h"

using emugl::ABORT_REASON_OTHER;
using emugl::FatalError;

#define POST_DEBUG 0
#if POST_DEBUG >= 1
#define DD(fmt, ...) \
    fprintf(stderr, "%s:%d| " fmt, __func__, __LINE__, ##__VA_ARGS__)
#else
#define DD(fmt, ...) (void)0
#endif

#define POST_ERROR(fmt, ...)                                                  \
    do {                                                                      \
        fprintf(stderr, "%s(%s:%d): " fmt "\n", __func__, __FILE__, __LINE__, \
                ##__VA_ARGS__);                                               \
        fflush(stderr);                                                       \
    } while (0)

static void sDefaultRunOnUiThread(UiUpdateFunc f, void* data, bool wait) {
    (void)f;
    (void)data;
    (void)wait;
}

PostWorker::PostWorker(PostWorker::BindSubwinCallback&& cb, bool mainThreadPostingOnly,
                       Compositor* compositor, DisplayVk* displayVk)
    : mFb(FrameBuffer::getFB()),
      mBindSubwin(cb),
      m_mainThreadPostingOnly(mainThreadPostingOnly),
      m_runOnUiThread(m_mainThreadPostingOnly ? emugl::get_emugl_window_operations().runOnUiThread
                                              : sDefaultRunOnUiThread),
      m_compositor(compositor),
      m_displayVk(displayVk) {}

void PostWorker::fillMultiDisplayPostStruct(ComposeLayer* l,
                                            hwc_rect_t displayArea,
                                            hwc_frect_t cropArea,
                                            hwc_transform_t transform) {
    l->composeMode = HWC2_COMPOSITION_DEVICE;
    l->blendMode = HWC2_BLEND_MODE_NONE;
    l->transform = transform;
    l->alpha = 1.0;
    l->displayFrame = displayArea;
    l->crop = cropArea;
}

std::shared_future<void> PostWorker::postImpl(ColorBuffer* cb) {
    std::shared_future<void> completedFuture =
        std::async(std::launch::deferred, [] {}).share();
    completedFuture.wait();
    // bind the subwindow eglSurface
    if (!m_mainThreadPostingOnly && m_needsToRebindWindow) {
        m_needsToRebindWindow = !mBindSubwin();
        if (m_needsToRebindWindow) {
            // Do not proceed if fail to bind to the window.
            return completedFuture;
        }
    }

    if (m_displayVk) {
        const auto imageInfo = mFb->borrowColorBufferForDisplay(cb->getHndl());
        bool success;
        Compositor::CompositionFinishedWaitable waitForGpu;
        std::tie(success, waitForGpu) = m_displayVk->post(imageInfo.get());
        if (!success) {
            // Create swapChain and retry
            if (mBindSubwin()) {
                const auto imageInfo = mFb->borrowColorBufferForDisplay(cb->getHndl());
                std::tie(success, waitForGpu) = m_displayVk->post(imageInfo.get());
            }
            if (!success) {
                m_needsToRebindWindow = true;
                return completedFuture;
            }
            m_needsToRebindWindow = false;
        }
        return waitForGpu;
    }

    float dpr = mFb->getDpr();
    int windowWidth = mFb->windowWidth();
    int windowHeight = mFb->windowHeight();
    float px = mFb->getPx();
    float py = mFb->getPy();
    int zRot = mFb->getZrot();
    hwc_transform_t rotation = (hwc_transform_t)0;

    // Find the x and y values at the origin when "fully scrolled."
    // Multiply by 2 because the texture goes from -1 to 1, not 0 to 1.
    // Multiply the windowing coordinates by DPR because they ignore
    // DPR, but the viewport includes DPR.
    float fx = 2.f * (m_viewportWidth  - windowWidth  * dpr) / (float)m_viewportWidth;
    float fy = 2.f * (m_viewportHeight - windowHeight * dpr) / (float)m_viewportHeight;

    // finally, compute translation values
    float dx = px * fx;
    float dy = py * fy;

    if (emugl::get_emugl_multi_display_operations().isMultiDisplayEnabled()) {
        uint32_t combinedW, combinedH;
        emugl::get_emugl_multi_display_operations().getCombinedDisplaySize(&combinedW, &combinedH);
        mFb->getTextureDraw()->prepareForDrawLayer();
        int32_t start_id = -1, x, y;
        uint32_t id, w, h, c;
        while(emugl::get_emugl_multi_display_operations().getNextMultiDisplay(start_id, &id,
                                                                              &x, &y, &w, &h,
                                                                              nullptr, nullptr,
                                                                              &c)) {
            if ((id != 0) && (w == 0 || h == 0 || c == 0)) {
                start_id = id;
                continue;
            }
            ColorBuffer* multiDisplayCb = id == 0 ? cb : mFb->findColorBuffer(c).get();
            if (multiDisplayCb == nullptr) {
                start_id = id;
                continue;
            }
            ComposeLayer l;
            hwc_rect_t displayArea = { .left = (int)x,
                                       .top = (int)y,
                                       .right = (int)(x + w),
                                       .bottom = (int)(y + h) };
            hwc_frect_t cropArea = { .left = 0.0,
                                     .top = (float)multiDisplayCb->getHeight(),
                                     .right = (float)multiDisplayCb->getWidth(),
                                     .bottom = 0.0 };
            fillMultiDisplayPostStruct(&l, displayArea, cropArea, rotation);
            multiDisplayCb->postLayer(l, combinedW, combinedH);
            start_id = id;
        }
        mFb->getTextureDraw()->cleanupForDrawLayer();
    }
    else if (emugl::get_emugl_window_operations().isFolded()) {
        mFb->getTextureDraw()->prepareForDrawLayer();
        ComposeLayer l;
        int x, y, w, h;
        emugl::get_emugl_window_operations().getFoldedArea(&x, &y, &w, &h);
        hwc_rect_t displayArea = { .left = 0,
                                   .top = 0,
                                   .right = windowWidth,
                                   .bottom = windowHeight };
        hwc_frect_t cropArea = { .left = (float)x,
                                 .top = (float)(y + h),
                                 .right = (float)(x + w),
                                 .bottom = (float)y };
        switch ((int)zRot/90) {
            case 1:
                rotation = HWC_TRANSFORM_ROT_270;
                break;
            case 2:
                rotation = HWC_TRANSFORM_ROT_180;
                break;
            case 3:
                rotation = HWC_TRANSFORM_ROT_90;
                break;
            default: ;
        }

        fillMultiDisplayPostStruct(&l, displayArea, cropArea, rotation);
        cb->postLayer(l, m_viewportWidth/dpr, m_viewportHeight/dpr);
        mFb->getTextureDraw()->cleanupForDrawLayer();
    }
    else {
        // render the color buffer to the window and apply the overlay
        GLuint tex = cb->getViewportScaledTexture();
        cb->postWithOverlay(tex, zRot, dx, dy);
    }

    s_egl.eglSwapBuffers(mFb->getDisplay(), mFb->getWindowSurface());
    return completedFuture;
}

// Called whenever the subwindow needs a refresh (FrameBuffer::setupSubWindow).
// This rebinds the subwindow context (to account for
// when the refresh is a display change, for instance)
// and resets the posting viewport.
void PostWorker::viewportImpl(int width, int height) {
    if (!m_mainThreadPostingOnly) {
        // For GLES, we rebind the subwindow eglSurface unconditionally: this
        // could be from a display change, but we want to avoid binding
        // VkSurfaceKHR too frequently, because that's too expensive.
        if (!m_displayVk || m_needsToRebindWindow) {
            m_needsToRebindWindow = !mBindSubwin();
            if (m_needsToRebindWindow) {
                // Do not proceed if fail to bind to the window.
                return;
            }
        }
    }

    if (m_displayVk) {
        return;
    }

    float dpr = mFb->getDpr();
    m_viewportWidth = width * dpr;
    m_viewportHeight = height * dpr;
    s_gles2.glViewport(0, 0, m_viewportWidth, m_viewportHeight);
}

// Called when the subwindow refreshes, but there is no
// last posted color buffer to show to the user. Instead of
// displaying whatever happens to be in the back buffer,
// clear() is useful for outputting consistent colors.
void PostWorker::clearImpl() {
    if (m_displayVk) {
        GFXSTREAM_ABORT(FatalError(ABORT_REASON_OTHER))
            << "PostWorker with Vulkan doesn't support clear";
    }
#ifndef __linux__
    s_gles2.glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
                    GL_STENCIL_BUFFER_BIT);
    s_egl.eglSwapBuffers(mFb->getDisplay(), mFb->getWindowSurface());
#endif
}

std::shared_future<void> PostWorker::composeImpl(const FlatComposeRequest& composeRequest) {
    if (!isComposeTargetReady(composeRequest.targetHandle)) {
        ERR("The last composition on the target buffer hasn't completed.");
    }

    std::shared_future<void> completedFuture =
        std::async(std::launch::deferred, [] {}).share();
    completedFuture.wait();

    // bind the subwindow eglSurface
    if (!m_mainThreadPostingOnly && m_needsToRebindWindow) {
        m_needsToRebindWindow = !mBindSubwin();
        if (m_needsToRebindWindow) {
            // Do not proceed if fail to bind to the window.
            return completedFuture;
        }
    }

    Compositor::CompositionRequest compositorRequest = {};
    compositorRequest.target = mFb->borrowColorBufferForComposition(composeRequest.targetHandle,
                                                                    /*colorBufferIsTarget=*/true);
    for (const ComposeLayer& guestLayer : composeRequest.layers) {
        // Skip the ColorBuffer whose id is 0.
        if (!guestLayer.cbHandle) {
            continue;
        }
        auto& compositorLayer = compositorRequest.layers.emplace_back();
        compositorLayer.props = guestLayer;
        compositorLayer.source =
            mFb->borrowColorBufferForComposition(guestLayer.cbHandle,
                                                 /*colorBufferIsTarget=*/false);
    }

    return m_compositor->compose(compositorRequest);
}

void PostWorker::screenshot(
    ColorBuffer* cb,
    int width,
    int height,
    GLenum format,
    GLenum type,
    int rotation,
    void* pixels) {
    if (m_displayVk) {
        GFXSTREAM_ABORT(FatalError(ABORT_REASON_OTHER)) <<
                            "Screenshot not supported with native Vulkan swapchain enabled.";
    }
    cb->readPixelsScaled(
        width, height, format, type, rotation, pixels);
}

void PostWorker::block(std::promise<void> scheduledSignal, std::future<void> continueSignal) {
    // MSVC STL doesn't support not copyable std::packaged_task. As a workaround, we use the
    // copyable std::shared_ptr here.
    auto block = std::make_shared<Post::Block>(Post::Block{
        .scheduledSignal = std::move(scheduledSignal),
        .continueSignal = std::move(continueSignal),
    });
    runTask(std::packaged_task<void()>([block] {
        block->scheduledSignal.set_value();
        block->continueSignal.wait();
    }));
}

PostWorker::~PostWorker() {
    if (mFb->getDisplay() != EGL_NO_DISPLAY) {
        s_egl.eglMakeCurrent(mFb->getDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE,
                             EGL_NO_CONTEXT);
    }
}

void PostWorker::post(ColorBuffer* cb, std::unique_ptr<Post::CompletionCallback> postCallback) {
    auto packagedPostCallback = std::shared_ptr<Post::CompletionCallback>(std::move(postCallback));
    runTask(
        std::packaged_task<void()>([cb, packagedPostCallback, this] {
            auto completedFuture = postImpl(cb);
            (*packagedPostCallback)(completedFuture);
        }));
}

void PostWorker::viewport(int width, int height) {
    runTask(std::packaged_task<void()>(
        [width, height, this] { viewportImpl(width, height); }));
}

void PostWorker::compose(std::unique_ptr<FlatComposeRequest> composeRequest,
                         std::unique_ptr<Post::CompletionCallback> composeCallback) {
    // std::shared_ptr(std::move(...)) is WA for MSFT STL implementation bug:
    // https://developercommunity.visualstudio.com/t/unable-to-move-stdpackaged-task-into-any-stl-conta/108672
    auto packagedComposeCallback =
        std::shared_ptr<Post::CompletionCallback>(std::move(composeCallback));
    auto packagedComposeRequest = std::shared_ptr<FlatComposeRequest>(std::move(composeRequest));
    runTask(
        std::packaged_task<void()>([packagedComposeCallback, packagedComposeRequest, this] {
        auto completedFuture = composeImpl(*packagedComposeRequest);
        m_composeTargetToComposeFuture.emplace(packagedComposeRequest->targetHandle,
                                               completedFuture);
        (*packagedComposeCallback)(completedFuture);
        }));
}

void PostWorker::clear() {
    runTask(std::packaged_task<void()>([this] { clearImpl(); }));
}

void PostWorker::runTask(std::packaged_task<void()> task) {
    using Task = std::packaged_task<void()>;
    auto taskPtr = std::make_unique<Task>(std::move(task));
    if (m_mainThreadPostingOnly) {
        m_runOnUiThread(
            [](void* data) {
                std::unique_ptr<Task> taskPtr(reinterpret_cast<Task*>(data));
                (*taskPtr)();
            },
            taskPtr.release(), false);
    } else {
        (*taskPtr)();
    }
}

bool PostWorker::isComposeTargetReady(uint32_t targetHandle) {
    // Even if the target ColorBuffer has already been destroyed, the compose future should have
    // been waited and set to the ready state.
    for (auto i = m_composeTargetToComposeFuture.begin();
         i != m_composeTargetToComposeFuture.end();) {
        auto& composeFuture = i->second;
        if (composeFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            i = m_composeTargetToComposeFuture.erase(i);
        } else {
            i++;
        }
    }
    if (m_composeTargetToComposeFuture.find(targetHandle) == m_composeTargetToComposeFuture.end()) {
        return true;
    }
    return false;
}

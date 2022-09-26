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

#include "DisplayGl.h"

#include "DisplaySurfaceGl.h"
#include "OpenGLESDispatch/DispatchTables.h"
#include "OpenGLESDispatch/EGLDispatch.h"

namespace {

std::shared_future<void> getCompletedFuture() {
    std::shared_future<void> completedFuture =
        std::async(std::launch::deferred, [] {}).share();
    completedFuture.wait();
    return completedFuture;
}

}  // namespace

std::shared_future<void> DisplayGl::post(const Post& post) {
    const auto* surface = getBoundSurface();
    if (!surface) {
        return getCompletedFuture();
    }
    const auto* surfaceGl = static_cast<const DisplaySurfaceGl*>(surface->getImpl());

    std::optional<RecursiveScopedContextBind> contextBind;
    if (mUseBoundSurfaceContext) {
        contextBind.emplace(surfaceGl->getContextHelper());
        if (!contextBind->isOk()) {
            return getCompletedFuture();
        }
    }

    for (const PostLayer& layer : post.layers) {
        if (layer.layerOptions) {
            layer.colorBuffer->postLayer(*layer.layerOptions,
                                         post.frameWidth,
                                         post.frameHeight);
        } else if (layer.overlayOptions) {
            layer.colorBuffer->postWithOverlay(layer.colorBuffer->getViewportScaledTexture(),
                                               layer.overlayOptions->rotation,
                                               layer.overlayOptions->dx,
                                               layer.overlayOptions->dy);
        }
    }

    s_egl.eglSwapBuffers(surfaceGl->mDisplay, surfaceGl->mSurface);

    return getCompletedFuture();
}

void DisplayGl::viewport(int width, int height) {
    mViewportWidth = width;
    mViewportHeight = height;

    const auto* surface = getBoundSurface();
    if (!surface) {
        return;
    }

    std::optional<RecursiveScopedContextBind> contextBind;
    if (mUseBoundSurfaceContext) {
        const auto* surfaceGl = static_cast<const DisplaySurfaceGl*>(surface->getImpl());
        contextBind.emplace(surfaceGl->getContextHelper());
        if (!contextBind->isOk()) {
            return;
        }
    }

    s_gles2.glViewport(0, 0, mViewportWidth, mViewportHeight);
}

void DisplayGl::clear() {
    const auto* surface = getBoundSurface();
    if (!surface) {
        return;
    }
    const auto* surfaceGl = static_cast<const DisplaySurfaceGl*>(surface->getImpl());

    std::optional<RecursiveScopedContextBind> contextBind;
    if (mUseBoundSurfaceContext) {
        contextBind.emplace(surfaceGl->getContextHelper());
        if (!contextBind->isOk()) {
            return;
        }
    }

#ifndef __linux__
    s_gles2.glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    s_egl.eglSwapBuffers(surfaceGl->mDisplay, surfaceGl->mSurface);
#endif
}

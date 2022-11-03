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

#include <array>
#include <memory>
#include <string>
#include <optional>
#include <unordered_set>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES/gl.h>
#include <GLES3/gl3.h>

#include "ContextHelper.h"
#include "Compositor.h"
#include "CompositorGl.h"
#include "Display.h"
#include "DisplayGl.h"
#include "DisplaySurface.h"
#include "EmulatedEglContext.h"
#include "EmulatedEglConfig.h"
#include "EmulatedEglContext.h"
#include "EmulatedEglImage.h"
#include "EmulatedEglWindowSurface.h"
#include "OpenGLESDispatch/GLESv2Dispatch.h"
#include "TextureDraw.h"

#define EGL_NO_CONFIG ((EGLConfig)0)

class FrameBuffer;

namespace gfxstream {

class EmulationGl {
   public:
    static std::unique_ptr<EmulationGl> create(uint32_t width, uint32_t height,
                                               bool allowWindowSurface);

    ~EmulationGl();

    GLESDispatchMaxVersion getGlesMaxDispatchVersion() const;

    static const GLint* getGlesMaxContextAttribs();

    bool hasEglExtension(const std::string& ext) const;
    void getEglVersion(EGLint* major, EGLint* minor) const;

    void getGlesVersion(GLint* major, GLint* minor) const;
    const std::string& getGlesVendor() const { return mGlesVendor; }
    const std::string& getGlesRenderer() const { return mGlesRenderer; }
    const std::string& getGlesVersionString() const { return mGlesVersion; }
    const std::string& getGlesExtensionsString() const { return mGlesExtensions; }
    bool isGlesVulkanInteropSupported() const { return mGlesVulkanInteropSupported; }

    bool isFastBlitSupported() const;
    void disableFastBlitForTesting();

    bool isAsyncReadbackSupported() const;

    std::unique_ptr<DisplaySurface> createWindowSurface(uint32_t width,
                                                        uint32_t height,
                                                        EGLNativeWindowType window);

    const EmulatedEglConfigList& getEmulationEglConfigs() const { return *mEmulatedEglConfigs; }

    CompositorGl* getCompositor() { return mCompositorGl.get(); }

    DisplayGl* getDisplay() { return mDisplayGl.get(); }

    // TODO(b/233939967): Remove after adding ColorBufferGl and EmulationGl::createColorBuffer().
    TextureDraw* getTextureDraw() const { return mTextureDraw.get(); }

    using GlesUuid = std::array<uint8_t, GL_UUID_SIZE_EXT>;
    const std::optional<GlesUuid> getGlesDeviceUuid() const { return mGlesDeviceUuid; }

    void setUseBoundSurfaceContextForDisplay(bool use);

    std::unique_ptr<EmulatedEglContext> createEmulatedEglContext(
        uint32_t emulatedEglConfigIndex,
        const EmulatedEglContext* shareContext,
        GLESApi api,
        HandleType handle);

    std::unique_ptr<EmulatedEglContext> loadEmulatedEglContext(
        android::base::Stream* stream);

    std::unique_ptr<EmulatedEglImage> createEmulatedEglImage(
        EmulatedEglContext* context,
        EGLenum target,
        EGLClientBuffer buffer,
        HandleType handle);

    std::unique_ptr<EmulatedEglWindowSurface> createEmulatedEglWindowSurface(
        uint32_t emulatedConfigIndex,
        uint32_t width,
        uint32_t height,
        HandleType handle);

    std::unique_ptr<EmulatedEglWindowSurface> loadEmulatedEglWindowSurface(
        android::base::Stream* stream,
        const ColorBufferMap& colorBuffers,
        const EmulatedEglContextMap& contexts);

  private:
    // TODO(b/233939967): Remove this after fully transitioning to EmulationGl.
    friend class ::FrameBuffer;

    EmulationGl() = default;

    EGLDisplay mEglDisplay = EGL_NO_DISPLAY;
    EGLint mEglVersionMajor = 0;
    EGLint mEglVersionMinor = 0;
    std::string mEglVendor;
    std::unordered_set<std::string> mEglExtensions;
    EGLConfig mEglConfig = EGL_NO_CONFIG;

    // The "global" context that all other contexts are shared with.
    EGLContext mEglContext = EGL_NO_CONTEXT;

    // Used for ColorBuffer ops.
    std::unique_ptr<gfxstream::DisplaySurface> mPbufferSurface;

    // Used for Composition and Display ops.
    std::unique_ptr<gfxstream::DisplaySurface> mWindowSurface;
    std::unique_ptr<gfxstream::DisplaySurface> mFakeWindowSurface;

    GLint mGlesVersionMajor = 0;
    GLint mGlesVersionMinor = 0;
    GLESDispatchMaxVersion mGlesDispatchMaxVersion = GLES_DISPATCH_MAX_VERSION_2;
    std::string mGlesVendor;
    std::string mGlesRenderer;
    std::string mGlesVersion;
    std::string mGlesExtensions;
    std::optional<GlesUuid> mGlesDeviceUuid;
    bool mGlesVulkanInteropSupported = false;

    std::unique_ptr<EmulatedEglConfigList> mEmulatedEglConfigs;

    bool mFastBlitSupported = false;

    std::unique_ptr<CompositorGl> mCompositorGl;
    std::unique_ptr<DisplayGl> mDisplayGl;

    std::unique_ptr<TextureDraw> mTextureDraw;
};

}  // namespace gfxstream

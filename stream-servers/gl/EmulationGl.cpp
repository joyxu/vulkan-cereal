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

#include "EmulationGl.h"

#include <algorithm>
#include <cstring>
#include <optional>
#include <vector>

#include "DisplaySurfaceGl.h"
#include "GLESVersionDetector.h"
#include "OpenGLESDispatch/DispatchTables.h"
#include "OpenGLESDispatch/EGLDispatch.h"
#include "OpenGLESDispatch/GLESv2Dispatch.h"
#include "RenderThreadInfoGl.h"
#include "aemu/base/misc/StringUtils.h"
#include "host-common/GfxstreamFatalError.h"
#include "host-common/feature_control.h"
#include "host-common/logging.h"
#include "host-common/misc.h"

namespace gfxstream {
namespace {

static void EGLAPIENTRY EglDebugCallback(EGLenum error,
                                         const char *command,
                                         EGLint messageType,
                                         EGLLabelKHR threadLabel,
                                         EGLLabelKHR objectLabel,
                                         const char *message) {
    GL_LOG("command:%s message:%s", command, message);
}

static void GL_APIENTRY GlDebugCallback(GLenum source,
                                        GLenum type,
                                        GLuint id,
                                        GLenum severity,
                                        GLsizei length,
                                        const GLchar *message,
                                        const void *userParam) {
    GL_LOG("message:%s", message);
}

static const GLint kGles2ContextAttribsESOrGLCompat[] = {
    EGL_CONTEXT_CLIENT_VERSION, 2,  //
    EGL_NONE,                       //
};

static const GLint kGles2ContextAttribsCoreGL[] = {
    EGL_CONTEXT_CLIENT_VERSION, 2,                                                 //
    EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,  //
    EGL_NONE,                                                                      //
};

static const GLint kGles3ContextAttribsESOrGLCompat[] = {
    EGL_CONTEXT_CLIENT_VERSION, 3,  //
    EGL_NONE,                       //
};

static const GLint kGles3ContextAttribsCoreGL[] = {
    EGL_CONTEXT_CLIENT_VERSION, 3,                                                 //
    EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,  //
    EGL_NONE,                                                                      //
};

static bool validateGles2Context(EGLDisplay display) {
    const GLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE,
    };

    EGLint numConfigs = 0;
    EGLConfig config;
    if (!s_egl.eglChooseConfig(display, configAttribs, &config, 1, &numConfigs)) {
        ERR("Failed to find GLES 2.x config.");
        return false;
    }
    if (numConfigs != 1) {
        ERR("Failed to find exactly 1 GLES 2.x config: found %d.", numConfigs);
        return false;
    }

    const EGLint surfaceAttribs[] = {
        EGL_WIDTH, 1,
        EGL_HEIGHT, 1,
        EGL_NONE,
    };

    EGLSurface surface = s_egl.eglCreatePbufferSurface(display, config, surfaceAttribs);
    if (surface == EGL_NO_SURFACE) {
        ERR("Failed to create GLES 2.x pbuffer surface.");
        return false;
    }

    const GLint* contextAttribs = EmulationGl::getGlesMaxContextAttribs();
    EGLContext context = s_egl.eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);
    if (context == EGL_NO_CONTEXT) {
        ERR("Failed to create GLES 2.x context.");
        s_egl.eglDestroySurface(display, surface);
        return false;
    }

    if (!s_egl.eglMakeCurrent(display, surface, surface, context)) {
        ERR("Failed to make GLES 2.x context current.");
        s_egl.eglDestroySurface(display, surface);
        s_egl.eglDestroyContext(display, context);
        return false;
    }

    const char* extensions = (const char*)s_gles2.glGetString(GL_EXTENSIONS);
    if (extensions == nullptr) {
        ERR("Failed to query GLES 2.x context extensions.");
        s_egl.eglDestroySurface(display, surface);
        s_egl.eglDestroyContext(display, context);
        return false;
    }

    // It is rare but some drivers actually fail this...
    if (!s_egl.eglMakeCurrent(display, EGL_NO_CONTEXT, EGL_NO_SURFACE, EGL_NO_SURFACE)) {
        ERR("Failed to unbind GLES 2.x context.");
        s_egl.eglDestroySurface(display, surface);
        s_egl.eglDestroyContext(display, context);
        return false;
    }

    s_egl.eglDestroyContext(display, context);
    s_egl.eglDestroySurface(display, surface);
    return true;
}

static std::optional<EGLConfig> getEmulationEglConfig(EGLDisplay display, bool allowWindowSurface) {
    GLint surfaceType = EGL_PBUFFER_BIT;

    if (allowWindowSurface) {
        surfaceType |= EGL_WINDOW_BIT;
    }

    // On Linux, we need RGB888 exactly, or eglMakeCurrent will fail,
    // as glXMakeContextCurrent needs to match the format of the
    // native pixmap.
    constexpr const EGLint kWantedRedSize = 8;
    constexpr const EGLint kWantedGreenSize = 8;
    constexpr const EGLint kWantedBlueSize = 8;

    const GLint configAttribs[] = {
        EGL_RED_SIZE, kWantedRedSize,             //
        EGL_GREEN_SIZE, kWantedGreenSize,         //
        EGL_BLUE_SIZE, kWantedBlueSize,           //
        EGL_SURFACE_TYPE, surfaceType,            //
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,  //
        EGL_NONE,                                 //
    };

    EGLint numConfigs = 0;
    s_egl.eglGetConfigs(display, nullptr, 0, &numConfigs);

    std::vector<EGLConfig> configs(numConfigs);

    EGLint numMatchedConfigs = 0;
    s_egl.eglChooseConfig(display, configAttribs, configs.data(), numConfigs, &numMatchedConfigs);

    configs.resize(numMatchedConfigs);

    for (EGLConfig config : configs) {
        EGLint foundRedSize = 0;
        s_egl.eglGetConfigAttrib(display, config, EGL_RED_SIZE, &foundRedSize);
        if (foundRedSize != kWantedRedSize) {
            continue;
        }

        EGLint foundGreenSize = 0;
        s_egl.eglGetConfigAttrib(display, config, EGL_GREEN_SIZE, &foundGreenSize);
        if (foundGreenSize != kWantedGreenSize) {
            continue;
        }

        EGLint foundBlueSize = 0;
        s_egl.eglGetConfigAttrib(display, config, EGL_BLUE_SIZE, &foundBlueSize);
        if (foundBlueSize != kWantedBlueSize) {
            continue;
        }

        return config;
    }

    return std::nullopt;
}

}  // namespace

std::unique_ptr<EmulationGl> EmulationGl::create(uint32_t width,
                                                 uint32_t height,
                                                 bool egl2egl,
                                                 bool allowWindowSurface) {
    std::unique_ptr<EmulationGl> emulationGl(new EmulationGl());

    if (s_egl.eglUseOsEglApi) {
        EGLBoolean useNullBackend = EGL_FALSE;
        if (egl2egl && feature_is_enabled(kFeature_VulkanNativeSwapchain)) {
            useNullBackend = EGL_TRUE;
        }
        s_egl.eglUseOsEglApi(egl2egl, useNullBackend);
    }

    emulationGl->mEglDisplay = s_egl.eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (emulationGl->mEglDisplay == EGL_NO_DISPLAY) {
        ERR("Failed to get EGL display.");
        return nullptr;
    }

    GL_LOG("call eglInitialize");
    if (!s_egl.eglInitialize(emulationGl->mEglDisplay,
                             &emulationGl->mEglVersionMajor,
                             &emulationGl->mEglVersionMinor)) {
        ERR("Failed to eglInitialize.");
        return nullptr;
    }

    s_egl.eglBindAPI(EGL_OPENGL_ES_API);

#ifdef ENABLE_GL_LOG
    if (s_egl.eglDebugMessageControlKHR) {
        const EGLAttrib controls[] = {
            EGL_DEBUG_MSG_CRITICAL_KHR,
            EGL_TRUE,
            EGL_DEBUG_MSG_ERROR_KHR,
            EGL_TRUE,
            EGL_DEBUG_MSG_WARN_KHR,
            EGL_TRUE,
            EGL_DEBUG_MSG_INFO_KHR,
            EGL_FALSE,
            EGL_NONE,
            EGL_NONE,
        };

        if (s_egl.eglDebugMessageControlKHR(&EglDebugCallback, controls) == EGL_SUCCESS) {
            GL_LOG("Successfully set eglDebugMessageControlKHR");
        } else {
            GL_LOG("Failed to eglDebugMessageControlKHR");
        }
    } else {
        GL_LOG("eglDebugMessageControlKHR not available");
    }
#endif

    emulationGl->mEglVendor = s_egl.eglQueryString(emulationGl->mEglDisplay, EGL_VENDOR);

    const std::string eglExtensions = s_egl.eglQueryString(emulationGl->mEglDisplay, EGL_EXTENSIONS);
    android::base::split<std::string>(eglExtensions, " ",
                                      [&](const std::string& found) {
                                        emulationGl->mEglExtensions.insert(found);
                                      });

    if (!emulationGl->hasEglExtension("EGL_KHR_gl_texture_2D_image")) {
        ERR("Failed to find required EGL_KHR_gl_texture_2D_image extension.");
        return nullptr;
    }

    emulationGl->mGlesDispatchMaxVersion
        = calcMaxVersionFromDispatch(emulationGl->mEglDisplay);
    if (s_egl.eglSetMaxGLESVersion) {
        // eglSetMaxGLESVersion must be called before any context binding
        // because it changes how we initialize the dispatcher table.
        s_egl.eglSetMaxGLESVersion(emulationGl->mGlesDispatchMaxVersion);
    }

    int glesVersionMajor;
    int glesVersionMinor;
    emugl::getGlesVersion(&glesVersionMajor, &glesVersionMinor);
    emulationGl->mGlesVersionMajor = glesVersionMajor;
    emulationGl->mGlesVersionMinor = glesVersionMinor;

    if (!validateGles2Context(emulationGl->mEglDisplay)) {
        ERR("Failed to validate creating GLES 2.x context.");
        return nullptr;
    }

    // TODO (b/207426737): Remove the Imagination-specific workaround.
    const bool disableFastBlit =
        emulationGl->mEglVendor.find("Imagination Technologies") != std::string::npos;

    emulationGl->mFastBlitSupported =
        (emulationGl->mGlesDispatchMaxVersion > GLES_DISPATCH_MAX_VERSION_2) &&
        !disableFastBlit &&
        (emugl::getRenderer() == SELECTED_RENDERER_HOST ||
         emugl::getRenderer() == SELECTED_RENDERER_SWIFTSHADER_INDIRECT ||
         emugl::getRenderer() == SELECTED_RENDERER_ANGLE_INDIRECT);

    auto eglConfigOpt = getEmulationEglConfig(emulationGl->mEglDisplay, allowWindowSurface);
    if (!eglConfigOpt) {
        ERR("Failed to find config for emulation GL.");
        return nullptr;
    }
    emulationGl->mEglConfig = *eglConfigOpt;

    const GLint* maxContextAttribs = getGlesMaxContextAttribs();

    emulationGl->mEglContext = s_egl.eglCreateContext(emulationGl->mEglDisplay,
                                                      emulationGl->mEglConfig,
                                                      EGL_NO_CONTEXT,
                                                      maxContextAttribs);
    if (emulationGl->mEglContext == EGL_NO_CONTEXT) {
        ERR("Failed to create context, error 0x%x.", s_egl.eglGetError());
        return nullptr;
    }

    // Create another context which shares with the default context to be
    // used when we bind the pbuffer. This prevents switching the drawable
    // binding back and forth on the framebuffer context.
    // The main purpose of it is to solve a "blanking" behaviour we see on
    // on Mac platform when switching binded drawable for a context however
    // it is more efficient on other platforms as well.
    auto pbufferSurfaceGl = DisplaySurfaceGl::createPbufferSurface(emulationGl->mEglDisplay,
                                                                   emulationGl->mEglConfig,
                                                                   emulationGl->mEglContext,
                                                                   maxContextAttribs,
                                                                   /*width=*/1,
                                                                   /*height=*/1);
    if (!pbufferSurfaceGl) {
        ERR("Failed to create pbuffer display surface.");
        return nullptr;
    }
    auto* pbufferSurfaceGlPtr = pbufferSurfaceGl.get();

    emulationGl->mPbufferSurface = std::make_unique<gfxstream::DisplaySurface>(
        /*width=*/1,
        /*height=*/1,
        std::move(pbufferSurfaceGl));

    auto fakeWindowSurfaceGl = DisplaySurfaceGl::createPbufferSurface(emulationGl->mEglDisplay,
                                                                      emulationGl->mEglConfig,
                                                                      emulationGl->mEglContext,
                                                                      maxContextAttribs,
                                                                      width,
                                                                      height);
    if (!fakeWindowSurfaceGl) {
        ERR("Failed to create fake window display surface.");
        return nullptr;
    }
    emulationGl->mFakeWindowSurface = std::make_unique<gfxstream::DisplaySurface>(
        width,
        height,
        std::move(fakeWindowSurfaceGl));

    emulationGl->mEmulatedEglConfigs =
        std::make_unique<EmulatedEglConfigList>(emulationGl->mEglDisplay,
                                                emulationGl->mGlesDispatchMaxVersion);
    if (emulationGl->mEmulatedEglConfigs->empty()) {
        ERR("Failed to initialize emulated configs.");
        return nullptr;
    }

    const bool hasEsOrEs2Context =
        std::any_of(emulationGl->mEmulatedEglConfigs->begin(),
                    emulationGl->mEmulatedEglConfigs->end(),
                    [](const EmulatedEglConfig& config) {
                        const GLint renderableType = config.getRenderableType();
                        return renderableType & (EGL_OPENGL_ES_BIT | EGL_OPENGL_ES2_BIT);
                    });
    if (!hasEsOrEs2Context) {
        ERR("Failed to find any usable guest EGL configs.");
        return nullptr;
    }

    RecursiveScopedContextBind contextBind(pbufferSurfaceGlPtr->getContextHelper());
    if (!contextBind.isOk()) {
        ERR("Failed to make pbuffer context and surface current");
        return nullptr;
    }

#ifdef ENABLE_GL_LOG
    bool debugSetup = false;
    if (s_gles2.glDebugMessageCallback) {
        s_gles2.glEnable(GL_DEBUG_OUTPUT);
        s_gles2.glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        s_gles2.glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
                                      GL_DEBUG_SEVERITY_HIGH, 0, nullptr, GL_TRUE);
        s_gles2.glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
                                      GL_DEBUG_SEVERITY_MEDIUM, 0, nullptr, GL_TRUE);
        s_gles2.glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
                                      GL_DEBUG_SEVERITY_LOW, 0, nullptr, GL_TRUE);
        s_gles2.glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
                                      GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr,
                                      GL_TRUE);
        s_gles2.glDebugMessageCallback(&GlDebugCallback, nullptr);
        debugSetup = s_gles2.glGetError() == GL_NO_ERROR;
        if (!debugSetup) {
            ERR("Failed to set up glDebugMessageCallback");
        } else {
            GL_LOG("Successfully set up glDebugMessageCallback");
        }
    }
    if (s_gles2.glDebugMessageCallbackKHR && !debugSetup) {
        s_gles2.glDebugMessageControlKHR(GL_DONT_CARE, GL_DONT_CARE,
                                         GL_DEBUG_SEVERITY_HIGH_KHR, 0, nullptr,
                                         GL_TRUE);
        s_gles2.glDebugMessageControlKHR(GL_DONT_CARE, GL_DONT_CARE,
                                         GL_DEBUG_SEVERITY_MEDIUM_KHR, 0, nullptr,
                                         GL_TRUE);
        s_gles2.glDebugMessageControlKHR(GL_DONT_CARE, GL_DONT_CARE,
                                         GL_DEBUG_SEVERITY_LOW_KHR, 0, nullptr,
                                         GL_TRUE);
        s_gles2.glDebugMessageControlKHR(GL_DONT_CARE, GL_DONT_CARE,
                                         GL_DEBUG_SEVERITY_NOTIFICATION_KHR, 0, nullptr,
                                         GL_TRUE);
        s_gles2.glDebugMessageCallbackKHR(&GlDebugCallback, nullptr);
        debugSetup = s_gles2.glGetError() == GL_NO_ERROR;
        if (!debugSetup) {
            ERR("Failed to set up glDebugMessageCallbackKHR");
        } else {
            GL_LOG("Successfully set up glDebugMessageCallbackKHR");
        }
    }
    if (!debugSetup) {
        GL_LOG("glDebugMessageCallback and glDebugMessageCallbackKHR not available");
    }
#endif

    emulationGl->mGlesVendor = (const char*)s_gles2.glGetString(GL_VENDOR);
    emulationGl->mGlesRenderer = (const char*)s_gles2.glGetString(GL_RENDERER);
    emulationGl->mGlesVersion = (const char*)s_gles2.glGetString(GL_VERSION);
    emulationGl->mGlesExtensions = (const char*)s_gles2.glGetString(GL_EXTENSIONS);

    s_gles2.glGetError();
    GLint numDeviceUuids = 0;
    s_gles2.glGetIntegerv(GL_NUM_DEVICE_UUIDS_EXT, &numDeviceUuids);
    if (numDeviceUuids == 1) {
        GlesUuid uuid{};
        s_gles2.glGetUnsignedBytei_vEXT(GL_DEVICE_UUID_EXT, 0, uuid.data());
        emulationGl->mGlesDeviceUuid = uuid;
    }

    emulationGl->mGlesVulkanInteropSupported = false;
    if (s_egl.eglQueryVulkanInteropSupportANDROID) {
        emulationGl->mGlesVulkanInteropSupported = s_egl.eglQueryVulkanInteropSupportANDROID();
    }

    emulationGl->mTextureDraw = std::make_unique<TextureDraw>();
    if (!emulationGl->mTextureDraw) {
        ERR("Failed to initialize TextureDraw.");
        return nullptr;
    }

    emulationGl->mCompositorGl = std::make_unique<CompositorGl>(emulationGl->mTextureDraw.get());
    emulationGl->mCompositorGl->bindToSurface(emulationGl->mFakeWindowSurface.get());

    emulationGl->mDisplayGl = std::make_unique<DisplayGl>();
    emulationGl->mDisplayGl->bindToSurface(emulationGl->mFakeWindowSurface.get());

    return emulationGl;
}

EmulationGl::~EmulationGl() {
    if (mCompositorGl) {
        mCompositorGl->unbindFromSurface();
    }
    if (mDisplayGl) {
        mDisplayGl->unbindFromSurface();
    }

    {
        const auto* displaySurfaceGl =
            reinterpret_cast<const DisplaySurfaceGl*>(mPbufferSurface->getImpl());

        RecursiveScopedContextBind contextBind(displaySurfaceGl->getContextHelper());
        if (!contextBind.isOk()) {
            mTextureDraw.reset();
        }
    }

    if (mEglDisplay != EGL_NO_DISPLAY) {
        s_egl.eglMakeCurrent(mEglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (mEglContext != EGL_NO_CONTEXT) {
            s_egl.eglDestroyContext(mEglDisplay, mEglContext);
            mEglContext = EGL_NO_CONTEXT;
        }
        mEglDisplay = EGL_NO_DISPLAY;
    }
}

/*static*/ const GLint* EmulationGl::getGlesMaxContextAttribs() {
    int glesMaj, glesMin;
    emugl::getGlesVersion(&glesMaj, &glesMin);
    if (shouldEnableCoreProfile()) {
        if (glesMaj == 2) {
            return kGles2ContextAttribsCoreGL;
        } else {
            return kGles3ContextAttribsCoreGL;
        }
    }
    if (glesMaj == 2) {
        return kGles2ContextAttribsESOrGLCompat;
    } else {
        return kGles3ContextAttribsESOrGLCompat;
    }
}

GLESDispatchMaxVersion EmulationGl::getGlesMaxDispatchVersion() const {
    return mGlesDispatchMaxVersion;
}

bool EmulationGl::hasEglExtension(const std::string& ext) const {
    return mEglExtensions.find(ext) != mEglExtensions.end();
}

void EmulationGl::getEglVersion(EGLint* major, EGLint* minor) const {
    if (major) {
        *major = mEglVersionMajor;
    }
    if (minor) {
        *minor = mEglVersionMinor;
    }
}

void EmulationGl::getGlesVersion(GLint* major, GLint* minor) const {
    if (major) {
        *major = mGlesVersionMajor;
    }
    if (minor) {
        *minor = mGlesVersionMinor;
    }
}

bool EmulationGl::isFastBlitSupported() const {
    return mFastBlitSupported;
}

void EmulationGl::disableFastBlitForTesting() {
    mFastBlitSupported = false;
}

bool EmulationGl::isAsyncReadbackSupported() const {
    return mGlesVersionMajor > 2;
}

std::unique_ptr<DisplaySurface> EmulationGl::createWindowSurface(
        uint32_t width,
        uint32_t height,
        EGLNativeWindowType window) {
    auto surfaceGl = DisplaySurfaceGl::createWindowSurface(mEglDisplay,
                                                           mEglConfig,
                                                           mEglContext,
                                                           getGlesMaxContextAttribs(),
                                                           window);
    if (!surfaceGl) {
        ERR("Failed to create DisplaySurfaceGl.");
        return nullptr;
    }

    return std::make_unique<DisplaySurface>(width,
                                            height,
                                            std::move(surfaceGl));
}

void EmulationGl::setUseBoundSurfaceContextForDisplay(bool use) {
    if (mDisplayGl) {
        mDisplayGl->setUseBoundSurfaceContext(use);
    }
    if (mCompositorGl) {
        mCompositorGl->setUseBoundSurfaceContext(use);
    }
}

}  // namespace gfxstream
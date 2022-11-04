/*
* Copyright (C) 2011-2015 The Android Open Source Project
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

#include "FrameBuffer.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include <iomanip>

#include "ContextHelper.h"
#include "GLESVersionDetector.h"
#include "Hwc2.h"
#include "NativeSubWindow.h"
#include "OpenGLESDispatch/DispatchTables.h"
#include "OpenGLESDispatch/EGLDispatch.h"
#include "RenderControl.h"
#include "RenderThreadInfo.h"
#include "RenderThreadInfoGl.h"
#include "SyncThread.h"
#include "aemu/base/LayoutResolver.h"
#include "aemu/base/synchronization/Lock.h"
#include "aemu/base/containers/Lookup.h"
#include "aemu/base/memory/MemoryTracker.h"
#include "aemu/base/Metrics.h"
#include "aemu/base/SharedLibrary.h"
#include "aemu/base/files/StreamSerializing.h"
#include "aemu/base/system/System.h"
#include "aemu/base/Tracing.h"
#include "gl/YUVConverter.h"
#include "gl/gles2_dec/gles2_dec.h"
#include "host-common/GfxstreamFatalError.h"
#include "host-common/crash_reporter.h"
#include "host-common/feature_control.h"
#include "host-common/logging.h"
#include "host-common/misc.h"
#include "host-common/vm_operations.h"
#include "vulkan/DisplayVk.h"
#include "vulkan/VkCommonOperations.h"
#include "vulkan/VkDecoderGlobalState.h"

using android::base::AutoLock;
using android::base::ManagedDescriptor;
using android::base::MetricEventVulkanOutOfMemory;
using android::base::Stream;
using android::base::WorkerProcessingResult;
using emugl::ABORT_REASON_OTHER;
using emugl::FatalError;
using emugl::GfxApiLogger;
using gfxstream::EmulatedEglContext;
using gfxstream::EmulatedEglContextMap;
using gfxstream::EmulatedEglContextPtr;
using gfxstream::EmulatedEglWindowSurface;
using gfxstream::EmulatedEglWindowSurfaceMap;
using gfxstream::EmulatedEglWindowSurfacePtr;
using gfxstream::GLESApi;
using gfxstream::GLESApi_CM;
using gfxstream::GLESApi_2;

// static std::string getTimeStampString() {
//     const time_t timestamp = android::base::getUnixTimeUs();
//     const struct tm *timeinfo = localtime(&timestamp);
//     // Target format: 07-31 4:44:33
//     char b[64];
//     snprintf(
//         b,
//         sizeof(b) - 1,
//         "%02u-%02u %02u:%02u:%02u",
//         timeinfo->tm_mon + 1,
//         timeinfo->tm_mday,
//         timeinfo->tm_hour,
//         timeinfo->tm_min,
//         timeinfo->tm_sec);
//     return std::string(b);
// }

// static unsigned int getUptimeMs() {
//     return android::base::getUptimeMs();
// }

static void dumpPerfStats() {
    // auto usage = System::get()->getMemUsage();
    // std::string memoryStats =
    //     emugl::getMemoryTracker()
    //             ? emugl::getMemoryTracker()->printUsage()
    //             : "";
    // auto cpuUsage = emugl::getCpuUsage();
    // std::string lastStats =
    //     cpuUsage ? cpuUsage->printUsage() : "";
    // printf("%s Uptime: %u ms Resident memory: %f mb %s \n%s\n",
    //     getTimeStampString().c_str(), getUptimeMs(),
    //     (float)usage.resident / 1048576.0f, lastStats.c_str(),
    //     memoryStats.c_str());
}

class PerfStatThread : public android::base::Thread {
public:
    PerfStatThread(bool* perfStatActive) :
      Thread(), m_perfStatActive(perfStatActive) {}

    virtual intptr_t main() {
      while (*m_perfStatActive) {
        sleepMs(1000);
        dumpPerfStats();
      }
      return 0;
    }

private:
    bool* m_perfStatActive;
};

FrameBuffer* FrameBuffer::s_theFrameBuffer = NULL;
HandleType FrameBuffer::s_nextHandle = 0;

static const GLint gles2ContextAttribsESOrGLCompat[] =
   { EGL_CONTEXT_CLIENT_VERSION, 2,
     EGL_NONE };

static const GLint gles2ContextAttribsCoreGL[] =
   { EGL_CONTEXT_CLIENT_VERSION, 2,
     EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR,
     EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
     EGL_NONE };

static const GLint gles3ContextAttribsESOrGLCompat[] =
   { EGL_CONTEXT_CLIENT_VERSION, 3,
     EGL_NONE };

static const GLint gles3ContextAttribsCoreGL[] =
   { EGL_CONTEXT_CLIENT_VERSION, 3,
     EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR,
     EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,
     EGL_NONE };

const GLint* getGlesMaxContextAttribs() {
    int glesMaj, glesMin;
    emugl::getGlesVersion(&glesMaj, &glesMin);
    if (shouldEnableCoreProfile()) {
        if (glesMaj == 2) {
            return gles2ContextAttribsCoreGL;
        } else {
            return gles3ContextAttribsCoreGL;
        }
    }
    if (glesMaj == 2) {
        return gles2ContextAttribsESOrGLCompat;
    } else {
        return gles3ContextAttribsESOrGLCompat;
    }
}

static char* getGLES2ExtensionString(EGLDisplay p_dpy) {
    EGLConfig config;
    EGLSurface surface;

    static const GLint configAttribs[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
                                          EGL_RENDERABLE_TYPE,
                                          EGL_OPENGL_ES2_BIT, EGL_NONE};

    int n;
    if (!s_egl.eglChooseConfig(p_dpy, configAttribs, &config, 1, &n) ||
        n == 0) {
        ERR("Could not find GLES 2.x config!");
        return NULL;
    }

    static const EGLint pbufAttribs[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};

    surface = s_egl.eglCreatePbufferSurface(p_dpy, config, pbufAttribs);
    if (surface == EGL_NO_SURFACE) {
        ERR("Could not create GLES 2.x Pbuffer!");
        return NULL;
    }

    EGLContext ctx = s_egl.eglCreateContext(p_dpy, config, EGL_NO_CONTEXT,
                                            getGlesMaxContextAttribs());
    if (ctx == EGL_NO_CONTEXT) {
        ERR("Could not create GLES 2.x Context!");
        s_egl.eglDestroySurface(p_dpy, surface);
        return NULL;
    }

    if (!s_egl.eglMakeCurrent(p_dpy, surface, surface, ctx)) {
        ERR("Could not make GLES 2.x context current!");
        s_egl.eglDestroySurface(p_dpy, surface);
        s_egl.eglDestroyContext(p_dpy, ctx);
        return NULL;
    }

    // the string pointer may become invalid when the context is destroyed
    const char* s = (const char*)s_gles2.glGetString(GL_EXTENSIONS);
    char* extString = strdup(s ? s : "");

    // It is rare but some drivers actually fail this...
    if (!s_egl.eglMakeCurrent(p_dpy, NULL, NULL, NULL)) {
        ERR("Could not unbind context. Please try updating graphics card driver!");
        free(extString);
        extString = NULL;
    }
    s_egl.eglDestroyContext(p_dpy, ctx);
    s_egl.eglDestroySurface(p_dpy, surface);

    return extString;
}

// A condition variable needed to wait for framebuffer initialization.
namespace {
struct InitializedGlobals {
    android::base::Lock lock;
    android::base::ConditionVariable condVar;
};
}  // namespace

// |sInitialized| caches the initialized framebuffer state - this way
// happy path doesn't need to lock the mutex.
static std::atomic<bool> sInitialized{false};
static InitializedGlobals* sGlobals() {
    static InitializedGlobals* g = new InitializedGlobals;
    return g;
}

void FrameBuffer::waitUntilInitialized() {
    if (sInitialized.load(std::memory_order_relaxed)) {
        return;
    }

#if SNAPSHOT_PROFILE > 1
    const auto startTime = android::base::getHighResTimeUs();
#endif
    {
        AutoLock l(sGlobals()->lock);
        sGlobals()->condVar.wait(
                &l, [] { return sInitialized.load(std::memory_order_acquire); });
    }
#if SNAPSHOT_PROFILE > 1
    printf("Waited for FrameBuffer initialization for %.03f ms\n",
           (android::base::getHighResTimeUs() - startTime) / 1000.0);
#endif
}

void FrameBuffer::finalize() {
    AutoLock lock(sGlobals()->lock);
    AutoLock fbLock(m_lock);
    m_perfStats = false;
    m_perfThread->wait(NULL);
    sInitialized.store(true, std::memory_order_relaxed);
    sGlobals()->condVar.broadcastAndUnlock(&lock);

    for (auto it : m_platformEglContexts) {
        destroySharedTrivialContext(it.second.context, it.second.surface);
    }

    if (m_shuttingDown) {
        // The only visible thing in the framebuffer is subwindow. Everything else
        // will get cleaned when the process exits.
        if (m_useSubWindow) {
            m_postWorker.reset();
            removeSubWindow_locked();
        }
        return;
    }

    sweepColorBuffersLocked();

    m_buffers.clear();
    {
        AutoLock lock(m_colorBufferMapLock);
        m_colorbuffers.clear();
    }
    m_colorBufferDelayedCloseList.clear();
    if (m_useSubWindow) {
        removeSubWindow_locked();
    }
    m_windows.clear();
    m_contexts.clear();

    m_readbackThread.enqueue({ReadbackCmd::Exit});
}

bool FrameBuffer::initialize(int width, int height, bool useSubWindow, bool egl2egl) {
    GL_LOG("FrameBuffer::initialize");
    if (s_theFrameBuffer != NULL) {
        return true;
    }

    android::base::initializeTracing();

    //
    // allocate space for the FrameBuffer object
    //
    std::unique_ptr<FrameBuffer> fb(new FrameBuffer(width, height, useSubWindow));
    if (!fb) {
        GL_LOG("Failed to create fb");
        ERR("Failed to create fb\n");
        return false;
    }

    std::unique_ptr<emugl::RenderDocWithMultipleVkInstances> renderDocMultipleVkInstances = nullptr;
    if (!android::base::getEnvironmentVariable("ANDROID_EMU_RENDERDOC").empty()) {
        SharedLibrary* renderdocLib = nullptr;
#ifdef _WIN32
        renderdocLib = SharedLibrary::open(R"(C:\Program Files\RenderDoc\renderdoc.dll)");
#elif defined(__linux__)
        renderdocLib = SharedLibrary::open("librenderdoc.so");
#endif
        fb->m_renderDoc = emugl::RenderDoc::create(renderdocLib);
        if (fb->m_renderDoc) {
            INFO("RenderDoc integration enabled.");
            renderDocMultipleVkInstances =
                std::make_unique<emugl::RenderDocWithMultipleVkInstances>(*fb->m_renderDoc);
            if (!renderDocMultipleVkInstances) {
                ERR("Failed to initialize RenderDoc with multiple VkInstances. Can't capture any "
                    "information from guest VkInstances with RenderDoc.");
            }
        }
    }
    // Initialize Vulkan emulation state
    //
    // Note: This must happen before any use of s_egl,
    // or it's possible that the existing EGL display and contexts
    // used by underlying EGL driver might become invalid,
    // preventing new contexts from being created that share
    // against those contexts.
    goldfish_vk::VkEmulation* vkEmu = nullptr;
    goldfish_vk::VulkanDispatch* vkDispatch = nullptr;
    if (feature_is_enabled(kFeature_Vulkan)) {
        vkDispatch = emugl::vkDispatch(false /* not for testing */);
        vkEmu = goldfish_vk::createGlobalVkEmulation(vkDispatch);
        if (!vkEmu) {
            ERR("Failed to initialize global Vulkan emulation. Disable the Vulkan support.");
        }
    }
    if (vkEmu) {
        fb->m_vulkanEnabled = true;
        if (feature_is_enabled(kFeature_VulkanNativeSwapchain)) {
            fb->m_vkInstance = vkEmu->instance;
        }
        if (vkEmu->deviceInfo.supportsIdProperties) {
            GL_LOG("Supports id properties, got a vulkan device UUID");
            fprintf(stderr, "%s: Supports id properties, got a vulkan device UUID\n", __func__);
            memcpy(fb->m_vulkanUUID.data(), vkEmu->deviceInfo.idProps.deviceUUID, VK_UUID_SIZE);
        } else {
            GL_LOG("Doesn't support id properties, no vulkan device UUID");
            fprintf(stderr, "%s: Doesn't support id properties, no vulkan device UUID\n", __func__);
        }
    }

    // Use ANGLE's EGL null backend to prevent from accidentally calling into EGL.
    if (s_egl.eglUseOsEglApi) {
        EGLBoolean useNullBackend = EGL_FALSE;
        if (egl2egl && feature_is_enabled(kFeature_VulkanNativeSwapchain)) {
            useNullBackend = EGL_TRUE;
        }
        s_egl.eglUseOsEglApi(egl2egl, useNullBackend);
    }

    // Do not initialize GL emulation if the guest is using ANGLE.
    if (!feature_is_enabled(kFeature_GuestUsesAngle)) {
        fb->m_emulationGl = gfxstream::EmulationGl::create(width, height, useSubWindow);
        if (!fb->m_emulationGl) {
            ERR("Failed to initialize GL emulation.");
            return false;
        }
    }

    fb->m_guestUsesAngle =
        feature_is_enabled(
            kFeature_GuestUsesAngle);

    fb->m_useVulkanComposition = feature_is_enabled(kFeature_GuestUsesAngle) ||
                                 feature_is_enabled(kFeature_VulkanNativeSwapchain);

    std::unique_ptr<goldfish_vk::VkEmulationFeatures> vkEmulationFeatures =
        std::make_unique<goldfish_vk::VkEmulationFeatures>(goldfish_vk::VkEmulationFeatures{
            .glInteropSupported = false,  // Set later.
            .deferredCommands =
                android::base::getEnvironmentVariable("ANDROID_EMU_VK_DISABLE_DEFERRED_COMMANDS")
                    .empty(),
            .createResourceWithRequirements =
                android::base::getEnvironmentVariable(
                    "ANDROID_EMU_VK_DISABLE_USE_CREATE_RESOURCES_WITH_REQUIREMENTS")
                    .empty(),
            .useVulkanComposition = fb->m_useVulkanComposition,
            .useVulkanNativeSwapchain = feature_is_enabled(kFeature_VulkanNativeSwapchain),
            .guestRenderDoc = std::move(renderDocMultipleVkInstances),
            .enableAstcLdrEmulation = feature_is_enabled(kFeature_VulkanAstcLdrEmulation),
            .enableEtc2Emulation = feature_is_enabled(kFeature_VulkanEtc2Emulation),
            .enableYcbcrEmulation = feature_is_enabled(kFeature_VulkanYcbcrEmulation),
            .guestUsesAngle = fb->m_guestUsesAngle,
        });

    //
    // Cache the GL strings so we don't have to think about threading or
    // current-context when asked for them.
    //
    bool useVulkanGraphicsDiagInfo =
        vkEmu && feature_is_enabled(kFeature_VulkanNativeSwapchain) && fb->m_guestUsesAngle;

    if (useVulkanGraphicsDiagInfo) {
        fb->m_graphicsAdapterVendor = vkEmu->deviceInfo.driverVendor;
        fb->m_graphicsAdapterName = vkEmu->deviceInfo.physdevProps.deviceName;

        uint32_t vkVersion = vkEmu->vulkanInstanceVersion;

        std::stringstream versionStringBuilder;
        versionStringBuilder << "Vulkan " << VK_API_VERSION_MAJOR(vkVersion) << "."
                             << VK_API_VERSION_MINOR(vkVersion) << "."
                             << VK_API_VERSION_PATCH(vkVersion) << " "
                             << vkEmu->deviceInfo.driverVendor << " "
                             << vkEmu->deviceInfo.driverVersion;
        fb->m_graphicsApiVersion = versionStringBuilder.str();

        std::stringstream instanceExtensionsStringBuilder;
        for (auto& ext : vkEmu->instanceExtensions) {
            if (instanceExtensionsStringBuilder.tellp() != 0) {
                instanceExtensionsStringBuilder << " ";
            }
            instanceExtensionsStringBuilder << ext.extensionName;
        }

        fb->m_graphicsApiExtensions = instanceExtensionsStringBuilder.str();

        std::stringstream deviceExtensionsStringBuilder;
        for (auto& ext : vkEmu->deviceInfo.extensions) {
            if (deviceExtensionsStringBuilder.tellp() != 0) {
                deviceExtensionsStringBuilder << " ";
            }
            deviceExtensionsStringBuilder << ext.extensionName;
        }

        fb->m_graphicsDeviceExtensions = deviceExtensionsStringBuilder.str();
    } else if (fb->m_emulationGl) {
        fb->m_graphicsAdapterVendor = fb->m_emulationGl->getGlesVendor();
        fb->m_graphicsAdapterName = fb->m_emulationGl->getGlesRenderer();
        fb->m_graphicsApiVersion = fb->m_emulationGl->getGlesVersionString();
        fb->m_graphicsApiExtensions = fb->m_emulationGl->getGlesExtensionsString();
        fb->m_graphicsDeviceExtensions = "N/A";
    } else {
        fb->m_graphicsAdapterVendor = "N/A";
        fb->m_graphicsAdapterName = "N/A";
        fb->m_graphicsApiVersion = "N/A";
        fb->m_graphicsApiExtensions = "N/A";
        fb->m_graphicsDeviceExtensions = "N/A";
    }

    // Attempt to get the device UUID of the gles and match with Vulkan. If
    // they match, interop is possible. If they don't, then don't trust the
    // result of interop query to egl and fall back to CPU copy, as we might
    // have initialized Vulkan devices and GLES contexts from different
    // physical devices.

    bool vulkanInteropSupported = true;
    // First, if the VkEmulation instance doesn't support ext memory capabilities,
    // it won't support uuids.
    if (!vkEmu || !vkEmu->deviceInfo.supportsIdProperties) {
        vulkanInteropSupported = false;
    }
    if (!fb->m_emulationGl) {
        vulkanInteropSupported = false;
    } else {
        if (!fb->m_emulationGl->isGlesVulkanInteropSupported()) {
            vulkanInteropSupported = false;
        }
        const auto& glesDeviceUuid = fb->m_emulationGl->getGlesDeviceUuid();
        if (!glesDeviceUuid  || glesDeviceUuid != fb->m_vulkanUUID) {
            vulkanInteropSupported = false;
        }
    }
    // TODO: 0-copy gl interop on swiftshader vk
    if (android::base::getEnvironmentVariable("ANDROID_EMU_VK_ICD") == "swiftshader") {
        vulkanInteropSupported = false;
        GL_LOG("vk icd swiftshader, disable interop");
    }

    fb->m_vulkanInteropSupported = vulkanInteropSupported;
    GL_LOG("interop? %d", fb->m_vulkanInteropSupported);

    GL_LOG("glvk interop final: %d", fb->m_vulkanInteropSupported);
    vkEmulationFeatures->glInteropSupported = fb->m_vulkanInteropSupported;
    if (feature_is_enabled(kFeature_Vulkan)) {
        goldfish_vk::initVkEmulationFeatures(std::move(vkEmulationFeatures));
        if (vkEmu->displayVk) {
            fb->m_displayVk = vkEmu->displayVk.get();
            fb->m_displaySurfaceUsers.push_back(fb->m_displayVk);
        }
    }

    if (fb->m_useVulkanComposition) {
        if (!vkEmu->compositorVk) {
            ERR("Failed to get CompositorVk from VkEmulation.");
            return false;
        }
        GL_LOG("Performing composition using CompositorVk.");
        fb->m_compositor = vkEmu->compositorVk.get();
    } else {
        GL_LOG("Performing composition using CompositorGl.");
        auto compositorGl = fb->m_emulationGl->getCompositor();
        fb->m_compositor = compositorGl;
        fb->m_displaySurfaceUsers.push_back(compositorGl);
    }

    if (fb->m_emulationGl) {
        auto displayGl = fb->m_emulationGl->getDisplay();
        fb->m_displayGl = displayGl;
        fb->m_displaySurfaceUsers.push_back(displayGl);
    }

    INFO("Graphics Adapter Vendor %s", fb->m_graphicsAdapterVendor.c_str());
    INFO("Graphics Adapter %s", fb->m_graphicsAdapterName.c_str());
    INFO("Graphics API Version %s", fb->m_graphicsApiVersion.c_str());
    INFO("Graphics API Extensions %s", fb->m_graphicsApiExtensions.c_str());
    INFO("Graphics Device Extensions %s", fb->m_graphicsDeviceExtensions.c_str());

    // Start up the single sync thread. If we are using Vulkan native
    // swapchain, then don't initialize SyncThread worker threads with EGL
    // contexts.
    SyncThread::initialize(
        /* noGL */ fb->m_displayVk != nullptr, fb->getHealthMonitor());

    //
    // Keep the singleton framebuffer pointer
    //
    s_theFrameBuffer = fb.release();
    {
        AutoLock lock(sGlobals()->lock);
        sInitialized.store(true, std::memory_order_release);
        sGlobals()->condVar.broadcastAndUnlock(&lock);
    }

    // Nothing else to do - we're ready to rock!
    return true;
}

bool FrameBuffer::importMemoryToColorBuffer(ManagedDescriptor externalDescriptor, uint64_t size,
                                            bool dedicated, bool vulkanOnly,
                                            uint32_t colorBufferHandle, VkImage image,
                                            const VkImageCreateInfo& imageCi) {
    AutoLock mutex(m_lock);

    ColorBufferPtr cb = findColorBuffer(colorBufferHandle);
    if (!cb) {
        // bad colorbuffer handle
        ERR("FB: importMemoryToColorBuffer cb handle %#x not found", colorBufferHandle);
        return false;
    }
    return cb->importMemory(std::move(externalDescriptor), size, dedicated,
                            imageCi.tiling == VK_IMAGE_TILING_LINEAR, vulkanOnly);
}

void FrameBuffer::setColorBufferInUse(
    uint32_t colorBufferHandle,
    bool inUse) {

    AutoLock mutex(m_lock);

    ColorBufferPtr colorBuffer = findColorBuffer(colorBufferHandle);
    if (!colorBuffer) {
        // bad colorbuffer handle
        ERR("FB: setColorBufferInUse cb handle %#x not found", colorBufferHandle);
        return;
    }

    colorBuffer->setInUse(inUse);
}

void FrameBuffer::fillGLESUsages(android_studio::EmulatorGLESUsages* usages) {
    if (s_egl.eglFillUsages) {
        s_egl.eglFillUsages(usages);
    }
}

FrameBuffer::FrameBuffer(int p_width, int p_height, bool useSubWindow)
    : m_framebufferWidth(p_width),
      m_framebufferHeight(p_height),
      m_windowWidth(p_width),
      m_windowHeight(p_height),
      m_useSubWindow(useSubWindow),
      m_fpsStats(getenv("SHOW_FPS_STATS") != nullptr),
      m_perfStats(!android::base::getEnvironmentVariable("SHOW_PERF_STATS").empty()),
      m_perfThread(new PerfStatThread(&m_perfStats)),
      m_readbackThread(
          [this](FrameBuffer::Readback&& readback) { return sendReadbackWorkerCmd(readback); }),
      m_refCountPipeEnabled(feature_is_enabled(kFeature_RefCountPipe)),
      m_noDelayCloseColorBufferEnabled(feature_is_enabled(kFeature_NoDelayCloseColorBuffer) ||
          feature_is_enabled(kFeature_Minigbm)),
      m_postThread([this](Post&& post) {
          return postWorkerFunc(post);
      }),
      m_logger(CreateMetricsLogger()),
      m_healthMonitor(*m_logger) {
    uint32_t displayId = 0;
    if (createDisplay(&displayId) < 0) {
        fprintf(stderr, "Failed to create default display\n");
    }

    setDisplayPose(displayId, 0, 0, getWidth(), getHeight(), 0);
    m_perfThread->start();
}

FrameBuffer::~FrameBuffer() {
    finalize();

    m_postThread.enqueue({
        PostCmd::Exit,
    });

    delete m_perfThread;

    if (s_theFrameBuffer) {
        s_theFrameBuffer = nullptr;
    }
    sInitialized.store(false, std::memory_order_relaxed);

    m_readbackThread.join();
    m_postThread.join();

    m_postWorker.reset();
    m_readbackWorker.reset();

    goldfish_vk::teardownGlobalVkEmulation();
    SyncThread::destroy();
}

WorkerProcessingResult
FrameBuffer::sendReadbackWorkerCmd(const Readback& readback) {
    ensureReadbackWorker();
    switch (readback.cmd) {
    case ReadbackCmd::Init:
        m_readbackWorker->initGL();
        return WorkerProcessingResult::Continue;
    case ReadbackCmd::GetPixels:
        m_readbackWorker->getPixels(readback.displayId, readback.pixelsOut, readback.bytes);
        return WorkerProcessingResult::Continue;
    case ReadbackCmd::AddRecordDisplay:
        m_readbackWorker->setRecordDisplay(readback.displayId, readback.width, readback.height, true);
        return WorkerProcessingResult::Continue;
    case ReadbackCmd::DelRecordDisplay:
        m_readbackWorker->setRecordDisplay(readback.displayId, 0, 0, false);
        return WorkerProcessingResult::Continue;
    case ReadbackCmd::Exit:
        return WorkerProcessingResult::Stop;
    }
    return WorkerProcessingResult::Stop;
}

WorkerProcessingResult FrameBuffer::postWorkerFunc(Post& post) {
    auto annotations = std::make_unique<EventHangMetadata::HangAnnotations>();
    annotations->insert({"Post command opcode", std::to_string(static_cast<uint64_t>(post.cmd))});
    auto watchdog = WATCHDOG_BUILDER(m_healthMonitor, "PostWorker main function")
                        .setAnnotations(std::move(annotations))
                        .build();
    switch (post.cmd) {
        case PostCmd::Post: {
            // We wrap the callback like this to workaround a bug in the MS STL implementation.
            auto packagePostCmdCallback =
                std::shared_ptr<Post::CompletionCallback>(std::move(post.completionCallback));
            std::unique_ptr<Post::CompletionCallback> postCallback =
                std::make_unique<Post::CompletionCallback>(
                    [packagePostCmdCallback](std::shared_future<void> waitForGpu) {
                        SyncThread::get()->triggerGeneral(
                            [composeCallback = std::move(packagePostCmdCallback), waitForGpu] {
                                (*composeCallback)(waitForGpu);
                            },
                            "Wait for post");
                    });
            m_postWorker->post(post.cb, std::move(postCallback));
            break;
        }
        case PostCmd::Viewport:
            m_postWorker->viewport(post.viewport.width,
                                   post.viewport.height);
            break;
        case PostCmd::Compose: {
            std::unique_ptr<FlatComposeRequest> composeRequest;
            std::unique_ptr<Post::CompletionCallback> composeCallback;
            if (post.composeVersion <= 1) {
                composeCallback = std::move(post.completionCallback);
                composeRequest = ToFlatComposeRequest((ComposeDevice*)post.composeBuffer.data());
            } else {
                // std::shared_ptr(std::move(...)) is WA for MSFT STL implementation bug:
                // https://developercommunity.visualstudio.com/t/unable-to-move-stdpackaged-task-into-any-stl-conta/108672
                auto packageComposeCallback =
                    std::shared_ptr<Post::CompletionCallback>(std::move(post.completionCallback));
                composeCallback = std::make_unique<Post::CompletionCallback>(
                    [packageComposeCallback](
                        std::shared_future<void> waitForGpu) {
                        SyncThread::get()->triggerGeneral(
                            [composeCallback = std::move(packageComposeCallback), waitForGpu] {
                                (*composeCallback)(waitForGpu);
                            },
                            "Wait for host composition");
                    });
                composeRequest = ToFlatComposeRequest((ComposeDevice_v2*)post.composeBuffer.data());
            }
            m_postWorker->compose(std::move(composeRequest), std::move(composeCallback));
            break;
        }
        case PostCmd::Clear:
            m_postWorker->clear();
            break;
        case PostCmd::Screenshot:
            m_postWorker->screenshot(
                post.screenshot.cb,
                post.screenshot.screenwidth,
                post.screenshot.screenheight,
                post.screenshot.format,
                post.screenshot.type,
                post.screenshot.rotation,
                post.screenshot.pixels);
            break;
        case PostCmd::Block:
            m_postWorker->block(std::move(post.block->scheduledSignal),
                                std::move(post.block->continueSignal));
            break;
        case PostCmd::Exit:
            return WorkerProcessingResult::Stop;
        default:
            break;
    }
    return WorkerProcessingResult::Continue;
}

std::future<void> FrameBuffer::sendPostWorkerCmd(Post post) {
#ifdef __APPLE__
    bool postOnlyOnMainThread = m_subWin && (emugl::getRenderer() == SELECTED_RENDERER_HOST);
#else
    bool postOnlyOnMainThread = false;
#endif

    bool expectedPostThreadStarted = false;
    if (m_postThreadStarted.compare_exchange_strong(expectedPostThreadStarted, true)) {
        if (m_emulationGl) {
            m_emulationGl->setUseBoundSurfaceContextForDisplay(!postOnlyOnMainThread);
        }

        m_postWorker.reset(new PostWorker(postOnlyOnMainThread,
                                          m_compositor,
                                          m_displayGl,
                                          m_displayVk));
        m_postThread.start();
    }

    // If we want to run only in the main thread and we are actually running
    // in the main thread already, don't use the PostWorker thread. Ideally,
    // PostWorker should handle this and dispatch directly, but we'll need to
    // transfer ownership of the thread to PostWorker.
    // TODO(lfy): do that refactor
    // For now, this fixes a screenshot issue on macOS.
    std::future<void> res = std::async(std::launch::deferred, [] {});
    res.wait();
    if (postOnlyOnMainThread && (PostCmd::Screenshot == post.cmd) &&
        emugl::get_emugl_window_operations().isRunningInUiThread()) {
        post.cb->readPixelsScaled(
            post.screenshot.screenwidth,
            post.screenshot.screenheight,
            post.screenshot.format,
            post.screenshot.type,
            post.screenshot.rotation,
            post.screenshot.pixels);
    } else {
        std::future<void> completeFuture =
            m_postThread.enqueue(Post(std::move(post)));
        if (!postOnlyOnMainThread ||
            (PostCmd::Screenshot == post.cmd &&
             !emugl::get_emugl_window_operations().isRunningInUiThread())) {
            res = std::move(completeFuture);
        }
    }
    return res;
}

void FrameBuffer::setPostCallback(
        emugl::Renderer::OnPostCallback onPost,
        void* onPostContext,
        uint32_t displayId,
        bool useBgraReadback) {
    AutoLock lock(m_lock);
    if (onPost) {
        uint32_t w, h;
        if (!emugl::get_emugl_multi_display_operations().getMultiDisplay(displayId,
                                                                         nullptr,
                                                                         nullptr,
                                                                         &w, &h,
                                                                         nullptr,
                                                                         nullptr,
                                                                         nullptr)) {
            ERR("display %d not exist, cancelling OnPost callback", displayId);
            return;
        }
        if (m_onPost.find(displayId) != m_onPost.end()) {
            ERR("display %d already configured for recording", displayId);
            return;
        }
        m_onPost[displayId].cb = onPost;
        m_onPost[displayId].context = onPostContext;
        m_onPost[displayId].displayId = displayId;
        m_onPost[displayId].width = w;
        m_onPost[displayId].height = h;
        m_onPost[displayId].img = new unsigned char[4 * w * h];
        m_onPost[displayId].readBgra = useBgraReadback;
        bool expectedReadbackThreadStarted = false;
        if (m_readbackThreadStarted.compare_exchange_strong(expectedReadbackThreadStarted, true)) {
            m_readbackThread.start();
            m_readbackThread.enqueue({ ReadbackCmd::Init });
        }
        std::future<void> completeFuture = m_readbackThread.enqueue(
            {ReadbackCmd::AddRecordDisplay, displayId, 0, nullptr, 0, w, h});
        completeFuture.wait();
    } else {
        std::future<void> completeFuture = m_readbackThread.enqueue(
            {ReadbackCmd::DelRecordDisplay, displayId});
        completeFuture.wait();
        m_onPost.erase(displayId);
    }
}

static void subWindowRepaint(void* param) {
    GL_LOG("call repost from subWindowRepaint callback");
    auto fb = static_cast<FrameBuffer*>(param);
    fb->repost();
}

bool FrameBuffer::setupSubWindow(FBNativeWindowType p_window,
                                 int wx,
                                 int wy,
                                 int ww,
                                 int wh,
                                 int fbw,
                                 int fbh,
                                 float dpr,
                                 float zRot,
                                 bool deleteExisting,
                                 bool hideWindow) {
    GL_LOG("Begin setupSubWindow");
    if (!m_useSubWindow) {
        ERR("%s: Cannot create native sub-window in this configuration\n",
            __FUNCTION__);
        return false;
    }

    // Do a quick check before even taking the lock - maybe we don't need to
    // do anything here.

    const bool createSubWindow = !m_subWin || deleteExisting;

    // On Mac, since window coordinates are Y-up and not Y-down, the
    // subwindow may not change dimensions, but because the main window
    // did, the subwindow technically needs to be re-positioned. This
    // can happen on rotation, so a change in Z-rotation can be checked
    // for this case. However, this *should not* be done on Windows/Linux,
    // because the functions used to resize a native window on those hosts
    // will block if the shape doesn't actually change, freezing the
    // emulator.
    const bool moveSubWindow =
            !createSubWindow && !(m_x == wx && m_y == wy &&
                                  m_windowWidth == ww && m_windowHeight == wh
#if defined(__APPLE__)
                                  && m_zRot == zRot
#endif
                                );

    const bool redrawSubwindow =
            createSubWindow || moveSubWindow || m_zRot != zRot || m_dpr != dpr;
    if (!createSubWindow && !moveSubWindow && !redrawSubwindow) {
        assert(sInitialized.load(std::memory_order_relaxed));
        GL_LOG("Exit setupSubWindow (nothing to do)");
#if SNAPSHOT_PROFILE > 1
        // printf("FrameBuffer::%s(): nothing to do at %lld ms\n", __func__,
               // (long long)System::get()->getProcessTimes().wallClockMs);
#endif
        return true;
    }

#if SNAPSHOT_PROFILE > 1
    // printf("FrameBuffer::%s(%s): start at %lld ms\n", __func__,
    //        deleteExisting ? "deleteExisting" : "keepExisting",
    //        (long long)System::get()->getProcessTimes().wallClockMs);
#endif

    class ScopedPromise {
       public:
        ~ScopedPromise() { mPromise.set_value(); }
        std::future<void> getFuture() { return mPromise.get_future(); }
        DISALLOW_COPY_ASSIGN_AND_MOVE(ScopedPromise);
        static std::tuple<std::unique_ptr<ScopedPromise>, std::future<void>> create() {
            auto scopedPromise = std::unique_ptr<ScopedPromise>(new ScopedPromise());
            auto future = scopedPromise->mPromise.get_future();
            return std::make_tuple(std::move(scopedPromise), std::move(future));
        }

       private:
        ScopedPromise() = default;
        std::promise<void> mPromise;
    };
    std::unique_ptr<ScopedPromise> postWorkerContinueSignal;
    std::future<void> postWorkerContinueSignalFuture;
    std::tie(postWorkerContinueSignal, postWorkerContinueSignalFuture) = ScopedPromise::create();
    {
        auto watchdog = WATCHDOG_BUILDER(m_healthMonitor, "Wait for other tasks on PostWorker")
                            .setTimeoutMs(6000)
                            .build();
        blockPostWorker(std::move(postWorkerContinueSignalFuture)).wait();
    }
    if (m_displayVk) {
        auto watchdog =
            WATCHDOG_BUILDER(m_healthMonitor, "Draining the VkQueue").setTimeoutMs(6000).build();
        m_displayVk->drainQueues();
    }
    HealthMonitor<>::Id lockWatchdogId =
        WATCHDOG_BUILDER(m_healthMonitor, "Wait for the FrameBuffer global lock")
            .build()
            ->release()
            .value();
    AutoLock mutex(m_lock);
    m_healthMonitor.stopMonitoringTask(lockWatchdogId);

#if SNAPSHOT_PROFILE > 1
    // printf("FrameBuffer::%s(): got lock at %lld ms\n", __func__,
    //        (long long)System::get()->getProcessTimes().wallClockMs);
#endif

    if (deleteExisting) {
        removeSubWindow_locked();
    }

    bool success = false;

    // If the subwindow doesn't exist, create it with the appropriate dimensions
    if (!m_subWin) {
        // Create native subwindow for FB display output
        m_x = wx;
        m_y = wy;
        m_windowWidth = ww;
        m_windowHeight = wh;

        m_subWin = ::createSubWindow(p_window, m_x, m_y, m_windowWidth,
                                     m_windowHeight, subWindowRepaint, this,
                                     hideWindow);
        if (m_subWin) {
            m_nativeWindow = p_window;



            if (m_displayVk) {
                m_displaySurface = goldfish_vk::createDisplaySurface(m_subWin,
                                                                     m_windowWidth,
                                                                     m_windowHeight);
            } else if (m_emulationGl) {
                m_displaySurface = m_emulationGl->createWindowSurface(m_windowWidth,
                                                                      m_windowHeight,
                                                                      m_subWin);
            } else {
                GFXSTREAM_ABORT(FatalError(ABORT_REASON_OTHER))
                    << "Unhandled window surface creation.";
            }

            if (m_displaySurface) {
                // Some backends use a default display surface. Unbind from that before
                // binding the new display surface. which potentially needs to be unbound.
                for (auto* displaySurfaceUser : m_displaySurfaceUsers) {
                    displaySurfaceUser->unbindFromSurface();
                }

                // TODO: Make RenderDoc a DisplaySurfaceUser.
                if (m_displayVk) {
                    if (m_renderDoc) {
                        m_renderDoc->call(emugl::RenderDoc::kSetActiveWindow,
                                          RENDERDOC_DEVICEPOINTER_FROM_VKINSTANCE(m_vkInstance),
                                          reinterpret_cast<RENDERDOC_WindowHandle>(m_subWin));
                    }
                }

                m_px = 0;
                m_py = 0;
                for (auto* displaySurfaceUser : m_displaySurfaceUsers) {
                    displaySurfaceUser->bindToSurface(m_displaySurface.get());
                }
                success = true;
            } else {
                // Display surface creation failed.
                if (m_emulationGl) {
                    // NOTE: This can typically happen with software-only renderers like OSMesa.
                    destroySubWindow(m_subWin);
                    m_subWin = (EGLNativeWindowType)0;
                } else {
                    GFXSTREAM_ABORT(FatalError(ABORT_REASON_OTHER))
                        << "Failed to create DisplaySurface.";
                }
            }
        }
    }

    auto watchdog = WATCHDOG_BUILDER(m_healthMonitor, "Updating subwindow state").build();
    // At this point, if the subwindow doesn't exist, it is because it either
    // couldn't be created
    // in the first place or the EGLSurface couldn't be created.
    if (m_subWin) {
        if (!moveSubWindow) {
            // Ensure that at least viewport parameters are properly updated.
            success = true;
        } else {
            // Only attempt to update window geometry if anything has actually
            // changed.
            m_x = wx;
            m_y = wy;
            m_windowWidth = ww;
            m_windowHeight = wh;

            {
                auto watchdog = WATCHDOG_BUILDER(m_healthMonitor, "Moving subwindow").build();
                success = ::moveSubWindow(m_nativeWindow, m_subWin, m_x, m_y, m_windowWidth,
                                          m_windowHeight);
            }
            m_displaySurface->updateSize(m_windowWidth, m_windowHeight);
        }
        // We are safe to unblock the PostWorker thread now, because we have completed all the
        // operations that could modify the state of the m_subWin. We need to unblock the PostWorker
        // here because we may need to send and wait for other tasks dispatched to the PostWorker
        // later, e.g. the viewport command or the post command issued later.
        postWorkerContinueSignal.reset();

        if (success && redrawSubwindow) {
            // Subwin creation or movement was successful,
            // update viewport and z rotation and draw
            // the last posted color buffer.
            m_dpr = dpr;
            m_zRot = zRot;
            if (m_displayVk == nullptr) {
                Post postCmd;
                postCmd.cmd = PostCmd::Viewport;
                postCmd.viewport.width = fbw;
                postCmd.viewport.height = fbh;
                std::future<void> completeFuture =
                    sendPostWorkerCmd(std::move(postCmd));
                completeFuture.wait();

                bool posted = false;

                if (m_lastPostedColorBuffer) {
                    GL_LOG("setupSubwindow: draw last posted cb");
                    posted = postImplSync(m_lastPostedColorBuffer, false);
                }

                if (!posted) {
                    Post postCmd;
                    postCmd.cmd = PostCmd::Clear;
                    std::future<void> completeFuture =
                        sendPostWorkerCmd(std::move(postCmd));
                    completeFuture.wait();
                }
            }
        }
    }

    if (m_emulationGl && success && redrawSubwindow) {
        RecursiveScopedContextBind bind(getPbufferSurfaceContextHelper());
        assert(bind.isOk());
        s_gles2.glViewport(0, 0, fbw * dpr, fbh * dpr);
    }
    mutex.unlock();

    // Nobody ever checks for the return code, so there will be no retries or
    // even aborted run; if we don't mark the framebuffer as initialized here
    // its users will hang forever; if we do mark it, they will crash - which
    // is a better outcome (crash report == bug fixed).
    AutoLock lock(sGlobals()->lock);
    sInitialized.store(true, std::memory_order_relaxed);
    sGlobals()->condVar.broadcastAndUnlock(&lock);

#if SNAPSHOT_PROFILE > 1
    // printf("FrameBuffer::%s(): end at %lld ms\n", __func__,
    //        (long long)System::get()->getProcessTimes().wallClockMs);
#endif

    GL_LOG("Exit setupSubWindow (successful setup)");
    return success;
}

bool FrameBuffer::removeSubWindow() {
    if (!m_useSubWindow) {
        ERR("Cannot remove native sub-window in this configuration");
        return false;
    }
    AutoLock lock(sGlobals()->lock);
    sInitialized.store(false, std::memory_order_relaxed);
    sGlobals()->condVar.broadcastAndUnlock(&lock);

    AutoLock mutex(m_lock);
    return removeSubWindow_locked();
}

bool FrameBuffer::removeSubWindow_locked() {
    if (!m_useSubWindow) {
        ERR("Cannot remove native sub-window in this configuration");
        return false;
    }
    bool removed = false;
    if (m_subWin) {
        for (auto* displaySurfaceUser : m_displaySurfaceUsers) {
            displaySurfaceUser->unbindFromSurface();
        }
        m_displaySurface.reset();

        destroySubWindow(m_subWin);

        m_subWin = (EGLNativeWindowType)0;
        removed = true;
    }
    return removed;
}

HandleType FrameBuffer::genHandle_locked() {
    HandleType id;
    do {
        id = ++s_nextHandle;
    } while (id == 0 || m_contexts.find(id) != m_contexts.end() ||
             m_windows.find(id) != m_windows.end() ||
             m_colorbuffers.find(id) != m_colorbuffers.end() ||
             m_buffers.find(id) != m_buffers.end());

    return id;
}

HandleType FrameBuffer::createColorBuffer(int p_width,
                                          int p_height,
                                          GLenum p_internalFormat,
                                          FrameworkFormat p_frameworkFormat) {

    AutoLock mutex(m_lock);
    sweepColorBuffersLocked();
    AutoLock colorBufferMapLock(m_colorBufferMapLock);

    return createColorBufferWithHandleLocked(p_width, p_height, p_internalFormat, p_frameworkFormat,
                                             genHandle_locked());
}

void FrameBuffer::createColorBufferWithHandle(int p_width, int p_height, GLenum p_internalFormat,
                                              FrameworkFormat p_frameworkFormat,
                                              HandleType handle) {
    {
        AutoLock mutex(m_lock);
        sweepColorBuffersLocked();

        AutoLock colorBufferMapLock(m_colorBufferMapLock);

        // Check for handle collision
        if (m_colorbuffers.count(handle) != 0) {
            // emugl::emugl_crash_reporter(
            //     "FATAL: color buffer with handle %u already exists",
            //     handle);
            GFXSTREAM_ABORT(FatalError(ABORT_REASON_OTHER));
        }

        handle = createColorBufferWithHandleLocked(p_width, p_height, p_internalFormat,
                                                   p_frameworkFormat, handle);
        if (!handle) {
            return;
        }
    }

    if (m_displayVk || m_guestUsesAngle) {
        goldfish_vk::setupVkColorBuffer(
            handle,
            m_guestUsesAngle /* not vulkan only */,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT /* memory property */,
            nullptr /* exported */);
    }
}

HandleType FrameBuffer::createColorBufferWithHandleLocked(
        int p_width,
        int p_height,
        GLenum p_internalFormat,
        FrameworkFormat p_frameworkFormat,
        HandleType handle) {
    EGLDisplay display = EGL_NO_DISPLAY;
    ContextHelper* contextHelper = nullptr;
    TextureDraw* textureDraw = nullptr;
    bool isFastBlitSupported = false;
    if (m_emulationGl) {
        display = getDisplay();
        contextHelper = getPbufferSurfaceContextHelper();
        textureDraw = getTextureDraw();
        isFastBlitSupported = this->isFastBlitSupported();
    }

    ColorBufferPtr cb(ColorBuffer::create(display, p_width, p_height,
                                          p_internalFormat, p_frameworkFormat,
                                          handle, contextHelper, textureDraw,
                                          isFastBlitSupported, m_guestUsesAngle));
    if (cb.get() != NULL) {
        assert(m_colorbuffers.count(handle) == 0);
        // When guest feature flag RefCountPipe is on, no reference counting is
        // needed. We only memoize the mapping from handle to ColorBuffer.
        // Explicitly set refcount to 1 to avoid the colorbuffer being added to
        // m_colorBufferDelayedCloseList in FrameBuffer::onLoad().
        if (m_refCountPipeEnabled) {
            m_colorbuffers.try_emplace(
                handle, ColorBufferRef{std::move(cb), 1, false, 0});
        } else {
            // Android master default api level is 1000
            int apiLevel = 1000;
            emugl::getAvdInfo(nullptr, &apiLevel);
            // pre-O and post-O use different color buffer memory management
            // logic
            if (apiLevel > 0 && apiLevel < 26) {
                m_colorbuffers.try_emplace(
                    handle,
                    ColorBufferRef{std::move(cb), 1, false, 0});

                RenderThreadInfo* tInfo = RenderThreadInfo::get();
                uint64_t puid = tInfo->m_puid;
                if (puid) {
                    m_procOwnedColorBuffers[puid].insert(handle);
                }

            } else {
                m_colorbuffers.try_emplace(
                    handle,
                    ColorBufferRef{std::move(cb), 0, false, 0});
            }
        }
    } else {
        ERR("Failed to create color buffer %d", handle);
        handle = 0;
    }
    return handle;
}

HandleType FrameBuffer::createBuffer(uint64_t p_size, uint32_t memoryProperty) {
    HandleType handle = 0;
    {
        AutoLock mutex(m_lock);
        // Hold the ColorBuffer map lock so that the new handle won't collide with a ColorBuffer
        // handle.
        AutoLock colorBufferMapLock(m_colorBufferMapLock);
        handle = createBufferWithHandleLocked(p_size, genHandle_locked());
    }

    bool setupStatus =
            goldfish_vk::setupVkBuffer(handle, /* vulkanOnly */ true, memoryProperty);
    assert(setupStatus);
    return handle;
}

void FrameBuffer::createBufferWithHandle(uint64_t size, HandleType handle) {
    {
        AutoLock mutex(m_lock);
        AutoLock colorBufferMapLock(m_colorBufferMapLock);

        // Check for handle collision
        if (m_buffers.count(handle) != 0) {
            GFXSTREAM_ABORT(FatalError(ABORT_REASON_OTHER))
                << "Buffer already exists with handle " << handle;
        }

        handle = createBufferWithHandleLocked(size, handle);
        if (!handle) {
            return;
        }
    }

    if (m_displayVk || m_guestUsesAngle) {
        goldfish_vk::setupVkBuffer(handle, /* vulkanOnly */ true,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }
}

HandleType FrameBuffer::createBufferWithHandleLocked(int p_size,
                                                     HandleType handle) {
    if (m_buffers.count(handle) != 0) {
        GFXSTREAM_ABORT(FatalError(ABORT_REASON_OTHER))
            << "Buffer already exists with handle " << handle;
    }

    BufferPtr buffer(Buffer::create(p_size, handle, getPbufferSurfaceContextHelper()));

    if (buffer) {
        m_buffers[handle] = {std::move(buffer)};
    } else {
        handle = 0;
        ERR("Create buffer failed.\n");
    }
    return handle;
}

HandleType FrameBuffer::createEmulatedEglContext(int config,
                                                 HandleType shareContextHandle,
                                                 GLESApi version) {
    if (!m_emulationGl) {
        GFXSTREAM_ABORT(FatalError(ABORT_REASON_OTHER))
            << "EGL emulation unavailable.";
    }

    AutoLock mutex(m_lock);
    android::base::AutoWriteLock contextLock(m_contextStructureLock);
    // Hold the ColorBuffer map lock so that the new handle won't collide with a ColorBuffer handle.
    AutoLock colorBufferMapLock(m_colorBufferMapLock);

    EmulatedEglContextPtr shareContext = nullptr;
    if (shareContextHandle != 0) {
        auto shareContextIt = m_contexts.find(shareContextHandle);
        if (shareContextIt == m_contexts.end()) {
            ERR("Failed to find share EmulatedEglContext:%d", shareContextHandle);
            return 0;
        }
        shareContext = shareContextIt->second;
    }

    HandleType contextHandle = genHandle_locked();
    auto context = m_emulationGl->createEmulatedEglContext(config,
                                                           shareContext.get(),
                                                           version,
                                                           contextHandle);
    if (!context) {
        ERR("Failed to create EmulatedEglContext.");
        return 0;
    }

    m_contexts[contextHandle] = std::move(context);

    RenderThreadInfo* tinfo = RenderThreadInfo::get();
    uint64_t puid = tinfo->m_puid;
    // The new emulator manages render contexts per guest process.
    // Fall back to per-thread management if the system image does not
    // support it.
    if (puid) {
        m_procOwnedEmulatedEglContexts[puid].insert(contextHandle);
    } else { // legacy path to manage context lifetime by threads
        if (!tinfo->m_glInfo) {
            GFXSTREAM_ABORT(FatalError(ABORT_REASON_OTHER))
                << "Render thread GL not available.";
        }
        tinfo->m_glInfo->m_contextSet.insert(contextHandle);
    }

    return contextHandle;
}

void FrameBuffer::destroyEmulatedEglContext(HandleType contextHandle) {
    AutoLock mutex(m_lock);
    sweepColorBuffersLocked();

    android::base::AutoWriteLock contextLock(m_contextStructureLock);
    m_contexts.erase(contextHandle);
    RenderThreadInfo* tinfo = RenderThreadInfo::get();
    uint64_t puid = tinfo->m_puid;
    // The new emulator manages render contexts per guest process.
    // Fall back to per-thread management if the system image does not
    // support it.
    if (puid) {
        auto it = m_procOwnedEmulatedEglContexts.find(puid);
        if (it != m_procOwnedEmulatedEglContexts.end()) {
            it->second.erase(contextHandle);
        }
    } else {
        if (!tinfo->m_glInfo) {
            GFXSTREAM_ABORT(FatalError(ABORT_REASON_OTHER))
                << "Render thread GL not available.";
        }
        tinfo->m_glInfo->m_contextSet.erase(contextHandle);
    }
}

HandleType FrameBuffer::createEmulatedEglWindowSurface(int p_config,
                                                       int p_width,
                                                       int p_height) {
    if (!m_emulationGl) {
        GFXSTREAM_ABORT(FatalError(ABORT_REASON_OTHER))
            << "EGL emulation unavailable.";
    }

    AutoLock mutex(m_lock);
    // Hold the ColorBuffer map lock so that the new handle won't collide with a ColorBuffer handle.
    AutoLock colorBufferMapLock(m_colorBufferMapLock);

    HandleType handle = genHandle_locked();

    auto window = m_emulationGl->createEmulatedEglWindowSurface(p_config,
                                                                p_width,
                                                                p_height,
                                                                handle);
    if (!window) {
        ERR("Failed to create EmulatedEglWindowSurface.");
        return 0;
    }

    m_windows[handle] = { std::move(window), 0 };

    RenderThreadInfo* info = RenderThreadInfo::get();
    if (!info->m_glInfo) {
        GFXSTREAM_ABORT(FatalError(ABORT_REASON_OTHER))
            << "RRenderThreadInfoGl not available.";
    }

    uint64_t puid = info->m_puid;
    if (puid) {
        m_procOwnedEmulatedEglWindowSurfaces[puid].insert(handle);
    } else { // legacy path to manage window surface lifetime by threads
        info->m_glInfo->m_windowSet.insert(handle);
    }

    return handle;
}

void FrameBuffer::destroyEmulatedEglWindowSurface(HandleType p_surface) {
    if (m_shuttingDown) {
        return;
    }
    AutoLock mutex(m_lock);
    auto colorBuffersToCleanup = destroyEmulatedEglWindowSurfaceLocked(p_surface);

    mutex.unlock();

    for (auto handle : colorBuffersToCleanup) {
        goldfish_vk::teardownVkColorBuffer(handle);
    }
}

std::vector<HandleType> FrameBuffer::destroyEmulatedEglWindowSurfaceLocked(HandleType p_surface) {
    std::vector<HandleType> colorBuffersToCleanUp;
    const auto w = m_windows.find(p_surface);
    if (w != m_windows.end()) {
        RecursiveScopedContextBind bind(getPbufferSurfaceContextHelper());
        if (!m_guestManagedColorBufferLifetime) {
            if (m_refCountPipeEnabled) {
                if (decColorBufferRefCountLocked(w->second.second)) {
                    colorBuffersToCleanUp.push_back(w->second.second);
                }
            } else {
                if (closeColorBufferLocked(w->second.second)) {
                    colorBuffersToCleanUp.push_back(w->second.second);
                }
            }
        }
        m_windows.erase(w);
        RenderThreadInfo* tinfo = RenderThreadInfo::get();
        uint64_t puid = tinfo->m_puid;
        if (puid) {
            auto ite = m_procOwnedEmulatedEglWindowSurfaces.find(puid);
            if (ite != m_procOwnedEmulatedEglWindowSurfaces.end()) {
                ite->second.erase(p_surface);
            }
        } else {
            if (!tinfo->m_glInfo) {
                GFXSTREAM_ABORT(FatalError(ABORT_REASON_OTHER))
                    << "Render thread GL not available.";
            }
            tinfo->m_glInfo->m_windowSet.erase(p_surface);
        }
    }
    return colorBuffersToCleanUp;
}

void FrameBuffer::drainGlRenderThreadResources() {
    // If we're already exiting then snapshot should not contain
    // this thread information at all.
    if (isShuttingDown()) {
        return;
    }

    // Release references to the current thread's context/surfaces if any
    bindContext(0, 0, 0);

    drainGlRenderThreadSurfaces();
    drainGlRenderThreadContexts();

    if (!s_egl.eglReleaseThread()) {
        ERR("Error: RenderThread @%p failed to eglReleaseThread()", this);
    }
}

void FrameBuffer::drainGlRenderThreadContexts() {
    if (isShuttingDown()) {
        return;
    }

    RenderThreadInfoGl* const tinfo = RenderThreadInfoGl::get();
    if (!tinfo) {
        GFXSTREAM_ABORT(FatalError(ABORT_REASON_OTHER))
            << "Render thread GL not available.";
    }

    if (tinfo->m_contextSet.empty()) {
        return;
    }

    AutoLock mutex(m_lock);
    android::base::AutoWriteLock contextLock(m_contextStructureLock);
    for (const HandleType contextHandle : tinfo->m_contextSet) {
        m_contexts.erase(contextHandle);
    }
    tinfo->m_contextSet.clear();
}

void FrameBuffer::drainGlRenderThreadSurfaces() {
    if (isShuttingDown()) {
        return;
    }

    RenderThreadInfoGl* const tinfo = RenderThreadInfoGl::get();
    if (!tinfo) {
        GFXSTREAM_ABORT(FatalError(ABORT_REASON_OTHER))
            << "Render thread GL not available.";
    }

    if (tinfo->m_windowSet.empty()) {
        return;
    }

    std::vector<HandleType> colorBuffersToCleanup;

    AutoLock mutex(m_lock);
    RecursiveScopedContextBind bind(getPbufferSurfaceContextHelper());
    for (const HandleType winHandle : tinfo->m_windowSet) {
        const auto winIt = m_windows.find(winHandle);
        if (winIt != m_windows.end()) {
            if (const HandleType oldColorBufferHandle = winIt->second.second) {
                if (!m_guestManagedColorBufferLifetime) {
                    if (m_refCountPipeEnabled) {
                        if (decColorBufferRefCountLocked(oldColorBufferHandle)) {
                            colorBuffersToCleanup.push_back(oldColorBufferHandle);
                        }
                    } else {
                        if (closeColorBufferLocked(oldColorBufferHandle)) {
                            colorBuffersToCleanup.push_back(oldColorBufferHandle);
                        }
                    }
                }
                m_windows.erase(winIt);
            }
        }
    }
    tinfo->m_windowSet.clear();

    m_lock.unlock();

    for (auto handle: colorBuffersToCleanup) {
        goldfish_vk::teardownVkColorBuffer(handle);
    }
}

int FrameBuffer::openColorBuffer(HandleType p_colorbuffer) {
    // When guest feature flag RefCountPipe is on, no reference counting is
    // needed.
    if (m_refCountPipeEnabled)
        return 0;

    RenderThreadInfo* tInfo = RenderThreadInfo::get();

    AutoLock mutex(m_lock);

    ColorBufferMap::iterator c;
    {
        AutoLock colorBuffermapLock(m_colorBufferMapLock);
        c = m_colorbuffers.find(p_colorbuffer);
        if (c == m_colorbuffers.end()) {
            // bad colorbuffer handle
            ERR("FB: openColorBuffer cb handle %#x not found", p_colorbuffer);
            return -1;
        }
        c->second.refcount++;
        markOpened(&c->second);
    }

    uint64_t puid = tInfo ? tInfo->m_puid : 0;
    if (puid) {
        m_procOwnedColorBuffers[puid].insert(p_colorbuffer);
    }
    return 0;
}

void FrameBuffer::closeColorBuffer(HandleType p_colorbuffer) {
    // When guest feature flag RefCountPipe is on, no reference counting is
    // needed.
    if (m_refCountPipeEnabled) {
        return;
    }

    RenderThreadInfo* tInfo = RenderThreadInfo::get();

    std::vector<HandleType> toCleanup;

    AutoLock mutex(m_lock);
    uint64_t puid = tInfo ? tInfo->m_puid : 0;
    if (puid) {
        auto ite = m_procOwnedColorBuffers.find(puid);
        if (ite != m_procOwnedColorBuffers.end()) {
            const auto& cb = ite->second.find(p_colorbuffer);
            if (cb != ite->second.end()) {
                ite->second.erase(cb);
                if (closeColorBufferLocked(p_colorbuffer)) {
                    toCleanup.push_back(p_colorbuffer);
                }
            }
        }
    } else {
        if (closeColorBufferLocked(p_colorbuffer)) {
            toCleanup.push_back(p_colorbuffer);
        }
    }

    mutex.unlock();

    for (auto handle : toCleanup) {
        goldfish_vk::teardownVkColorBuffer(handle);
    }
}

void FrameBuffer::closeBuffer(HandleType p_buffer) {
    AutoLock mutex(m_lock);

    if (m_buffers.find(p_buffer) == m_buffers.end()) {
        ERR("closeColorBuffer: cannot find buffer %u",
            static_cast<uint32_t>(p_buffer));
    } else {
        goldfish_vk::teardownVkBuffer(p_buffer);
        m_buffers.erase(p_buffer);
    }
}

bool FrameBuffer::closeColorBufferLocked(HandleType p_colorbuffer,
                                         bool forced) {
    // When guest feature flag RefCountPipe is on, no reference counting is
    // needed.
    if (m_refCountPipeEnabled) {
        return false;
    }
    bool deleted = false;
    {
        AutoLock colorBufferMapLock(m_colorBufferMapLock);

        if (m_noDelayCloseColorBufferEnabled) forced = true;

        ColorBufferMap::iterator c(m_colorbuffers.find(p_colorbuffer));
        if (c == m_colorbuffers.end()) {
            // This is harmless: it is normal for guest system to issue
            // closeColorBuffer command when the color buffer is already
            // garbage collected on the host. (we don't have a mechanism
            // to give guest a notice yet)
            return false;
        }

        // The guest can and will gralloc_alloc/gralloc_free and then
        // gralloc_register a buffer, due to API level (O+) or
        // timing issues.
        // So, we don't actually close the color buffer when refcount
        // reached zero, unless it has been opened at least once already.
        // Instead, put it on a 'delayed close' list to return to it later.
        if (--c->second.refcount == 0) {
            if (forced) {
                eraseDelayedCloseColorBufferLocked(c->first, c->second.closedTs);
                m_colorbuffers.erase(c);
                deleted = true;
            } else {
                c->second.closedTs = android::base::getUnixTimeUs();
                m_colorBufferDelayedCloseList.push_back({c->second.closedTs, p_colorbuffer});
            }
        }
    }

    performDelayedColorBufferCloseLocked(false);

    return deleted;
}

void FrameBuffer::performDelayedColorBufferCloseLocked(bool forced) {
    // Let's wait just long enough to make sure it's not because of instant
    // timestamp change (end of previous second -> beginning of a next one),
    // but not for long - this is a workaround for race conditions, and they
    // are quick.
    static constexpr int kColorBufferClosingDelaySec = 1;

    const auto now = android::base::getUnixTimeUs();
    auto it = m_colorBufferDelayedCloseList.begin();
    while (it != m_colorBufferDelayedCloseList.end() &&
           (forced ||
           it->ts + kColorBufferClosingDelaySec <= now)) {
        if (it->cbHandle != 0) {
            AutoLock colorBufferMapLock(m_colorBufferMapLock);
            const auto& cb = m_colorbuffers.find(it->cbHandle);
            if (cb != m_colorbuffers.end()) {
                m_colorbuffers.erase(cb);
            }
        }
        ++it;
    }
    m_colorBufferDelayedCloseList.erase(
                m_colorBufferDelayedCloseList.begin(), it);
}

void FrameBuffer::eraseDelayedCloseColorBufferLocked(
        HandleType cb, uint64_t ts)
{
    // Find the first delayed buffer with a timestamp <= |ts|
    auto it = std::lower_bound(
                  m_colorBufferDelayedCloseList.begin(),
                  m_colorBufferDelayedCloseList.end(), ts,
                  [](const ColorBufferCloseInfo& ci, uint64_t ts) {
        return ci.ts < ts;
    });
    while (it != m_colorBufferDelayedCloseList.end() &&
           it->ts == ts) {
        // if this is the one we need - clear it out.
        if (it->cbHandle == cb) {
            it->cbHandle = 0;
            break;
        }
        ++it;
    }
}

void FrameBuffer::createGraphicsProcessResources(uint64_t puid) {
    AutoLock mutex(m_lock);
    bool inserted = m_procOwnedResources.try_emplace(puid, ProcessResources::create()).second;
    if (!inserted) {
        WARN("Failed to create process resource for puid %" PRIu64 ".", puid);
    }
}

std::unique_ptr<ProcessResources> FrameBuffer::removeGraphicsProcessResources(uint64_t puid) {
    std::unordered_map<uint64_t, std::unique_ptr<ProcessResources>>::node_type node;
    {
        AutoLock mutex(m_lock);
        node = m_procOwnedResources.extract(puid);
    }
    if (node.empty()) {
        WARN("Failed to find process resource for puid %" PRIu64 ".", puid);
        return nullptr;
    }
    std::unique_ptr<ProcessResources> res = std::move(node.mapped());
    return res;
}

void FrameBuffer::cleanupProcGLObjects(uint64_t puid) {
    bool renderThreadWithThisPuidExists = false;

    do {
        renderThreadWithThisPuidExists = false;
        RenderThreadInfo::forAllRenderThreadInfos(
            [puid, &renderThreadWithThisPuidExists](RenderThreadInfo* i) {
            if (i->m_puid == puid) {
                renderThreadWithThisPuidExists = true;
            }
        });
        android::base::sleepUs(10000);
    } while (renderThreadWithThisPuidExists);


    AutoLock mutex(m_lock);
    if (!hasEmulationGl() || !getDisplay()) {
        return;
    }

    auto colorBuffersToCleanup = cleanupProcGLObjects_locked(puid);

    // Run other cleanup callbacks
    // Avoid deadlock by first storing a separate list of callbacks
    std::vector<std::function<void()>> callbacks;

    {
        auto procIte = m_procOwnedCleanupCallbacks.find(puid);
        if (procIte != m_procOwnedCleanupCallbacks.end()) {
            for (auto it : procIte->second) {
                callbacks.push_back(it.second);
            }
            m_procOwnedCleanupCallbacks.erase(procIte);
        }
    }

    mutex.unlock();

    for (auto handle : colorBuffersToCleanup) {
        goldfish_vk::teardownVkColorBuffer(handle);
    }

    for (auto cb : callbacks) {
        cb();
    }
}

std::vector<HandleType> FrameBuffer::cleanupProcGLObjects_locked(uint64_t puid, bool forced) {
    std::vector<HandleType> colorBuffersToCleanup;
    {
        std::unique_ptr<RecursiveScopedContextBind> bind = nullptr;
        if (m_emulationGl) {
            bind = std::make_unique<RecursiveScopedContextBind>(getPbufferSurfaceContextHelper());
        }
        // Clean up window surfaces
        {
            auto procIte = m_procOwnedEmulatedEglWindowSurfaces.find(puid);
            if (procIte != m_procOwnedEmulatedEglWindowSurfaces.end()) {
                for (auto whndl : procIte->second) {
                    auto w = m_windows.find(whndl);
                    if (!m_guestManagedColorBufferLifetime) {
                        if (m_refCountPipeEnabled) {
                            if (decColorBufferRefCountLocked(w->second.second)) {
                                colorBuffersToCleanup.push_back(w->second.second);
                            }
                        } else {
                            if (closeColorBufferLocked(w->second.second, forced)) {
                                colorBuffersToCleanup.push_back(w->second.second);
                            }
                        }
                    }
                    m_windows.erase(w);
                }
                m_procOwnedEmulatedEglWindowSurfaces.erase(procIte);
            }
        }
        // Clean up color buffers.
        // A color buffer needs to be closed as many times as it is opened by
        // the guest process, to give the correct reference count.
        // (Note that a color buffer can be shared across guest processes.)
        {
            if (!m_guestManagedColorBufferLifetime) {
                auto procIte = m_procOwnedColorBuffers.find(puid);
                if (procIte != m_procOwnedColorBuffers.end()) {
                    for (auto cb : procIte->second) {
                        if (closeColorBufferLocked(cb, forced)) {
                            colorBuffersToCleanup.push_back(cb);
                        }
                    }
                    m_procOwnedColorBuffers.erase(procIte);
                }
            }
        }

        // Clean up EGLImage handles
        {
            auto procIte = m_procOwnedEGLImages.find(puid);
            if (procIte != m_procOwnedEGLImages.end()) {
                if (!procIte->second.empty()) {
                    for (auto eglImg : procIte->second) {
                        s_egl.eglDestroyImageKHR(
                                getDisplay(),
                                reinterpret_cast<EGLImageKHR>((HandleType)eglImg));
                    }
                }
                m_procOwnedEGLImages.erase(procIte);
            }
        }
    }
    // Unbind before cleaning up contexts
    // Cleanup render contexts
    {
        auto procIte = m_procOwnedEmulatedEglContexts.find(puid);
        if (procIte != m_procOwnedEmulatedEglContexts.end()) {
            for (auto ctx : procIte->second) {
                m_contexts.erase(ctx);
            }
            m_procOwnedEmulatedEglContexts.erase(procIte);
        }
    }

    return colorBuffersToCleanup;
}

void FrameBuffer::markOpened(ColorBufferRef* cbRef) {
    cbRef->opened = true;
    eraseDelayedCloseColorBufferLocked(cbRef->cb->getHndl(), cbRef->closedTs);
    cbRef->closedTs = 0;
}

bool FrameBuffer::flushEmulatedEglWindowSurfaceColorBuffer(HandleType p_surface) {
    AutoLock mutex(m_lock);

    EmulatedEglWindowSurfaceMap::iterator w(m_windows.find(p_surface));
    if (w == m_windows.end()) {
        ERR("FB::flushEmulatedEglWindowSurfaceColorBuffer: window handle %#x not found",
            p_surface);
        // bad surface handle
        return false;
    }

    GLenum resetStatus = s_gles2.glGetGraphicsResetStatusEXT();
    if (resetStatus != GL_NO_ERROR) {
        GFXSTREAM_ABORT(FatalError(ABORT_REASON_OTHER)) <<
                "Stream server aborting due to graphics reset. ResetStatus: " <<
                std::hex << resetStatus;
    }

    EmulatedEglWindowSurface* surface = (*w).second.first.get();
    surface->flushColorBuffer();

    return true;
}

HandleType FrameBuffer::getEmulatedEglWindowSurfaceColorBufferHandle(HandleType p_surface) {
    AutoLock mutex(m_lock);

    auto it = m_EmulatedEglWindowSurfaceToColorBuffer.find(p_surface);
    if (it == m_EmulatedEglWindowSurfaceToColorBuffer.end()) {
        return 0;
    }

    return it->second;
}

bool FrameBuffer::setEmulatedEglWindowSurfaceColorBuffer(HandleType p_surface,
                                                         HandleType p_colorbuffer) {
    AutoLock mutex(m_lock);

    EmulatedEglWindowSurfaceMap::iterator w(m_windows.find(p_surface));
    if (w == m_windows.end()) {
        // bad surface handle
        ERR("bad window surface handle %#x", p_surface);
        return false;
    }

    {
        AutoLock colorBufferMapLock(m_colorBufferMapLock);
        ColorBufferMap::iterator c(m_colorbuffers.find(p_colorbuffer));
        if (c == m_colorbuffers.end()) {
            ERR("bad color buffer handle %#x", p_colorbuffer);
            // bad colorbuffer handle
            return false;
        }

        (*w).second.first->setColorBuffer((*c).second.cb);
        markOpened(&c->second);
        if (!m_guestManagedColorBufferLifetime) {
            c->second.refcount++;
        }
    }
    if (w->second.second) {
        if (!m_guestManagedColorBufferLifetime) {
            if (m_refCountPipeEnabled) {
                decColorBufferRefCountLocked(w->second.second);
            } else {
                closeColorBufferLocked(w->second.second);
            }
        }
    }

    (*w).second.second = p_colorbuffer;

    m_EmulatedEglWindowSurfaceToColorBuffer[p_surface] = p_colorbuffer;

    return true;
}

void FrameBuffer::readBuffer(HandleType handle, uint64_t offset, uint64_t size, void* bytes) {
    if (m_guestUsesAngle) {
        goldfish_vk::readBufferToBytes(handle, offset, size, bytes);
        return;
    }

    AutoLock mutex(m_lock);

    BufferPtr buffer = findBuffer(handle);
    if (!buffer) {
        ERR("Failed to read buffer: buffer %d not found.", handle);
        return;
    }

    buffer->read(offset, size, bytes);
}

void FrameBuffer::readColorBuffer(HandleType p_colorbuffer,
                                  int x,
                                  int y,
                                  int width,
                                  int height,
                                  GLenum format,
                                  GLenum type,
                                  void* pixels) {
    if (m_guestUsesAngle) {
        goldfish_vk::readColorBufferToBytes(p_colorbuffer, x, y, width, height, pixels);
        return;
    }

    AutoLock mutex(m_lock);

    ColorBufferPtr colorBuffer = findColorBuffer(p_colorbuffer);
    if (!colorBuffer) {
        // bad colorbuffer handle
        return;
    }

    colorBuffer->readPixels(x, y, width, height, format, type, pixels);
}

void FrameBuffer::readColorBufferYUV(HandleType p_colorbuffer,
                                     int x,
                                     int y,
                                     int width,
                                     int height,
                                     void* pixels,
                                     uint32_t pixels_size) {
    if (m_guestUsesAngle) {
        goldfish_vk::readColorBufferToBytes(p_colorbuffer, x, y, width, height, pixels);
        return;
    }

    AutoLock mutex(m_lock);

    ColorBufferPtr colorBuffer = findColorBuffer(p_colorbuffer);
    if (!colorBuffer) {
        // bad colorbuffer handle
        return;
    }

    colorBuffer->readPixelsYUVCached(x, y, width, height, pixels, pixels_size);
}

void FrameBuffer::createYUVTextures(uint32_t type,
                                    uint32_t count,
                                    int width,
                                    int height,
                                    uint32_t* output) {
    FrameworkFormat format = static_cast<FrameworkFormat>(type);
    AutoLock mutex(m_lock);
    RecursiveScopedContextBind bind(getPbufferSurfaceContextHelper());
    for (uint32_t i = 0; i < count; ++i) {
        if (format == FRAMEWORK_FORMAT_NV12) {
            YUVConverter::createYUVGLTex(GL_TEXTURE0, width, height,
                                         format, YUVPlane::Y, &output[2 * i]);
            YUVConverter::createYUVGLTex(GL_TEXTURE1, width / 2, height / 2,
                                         format, YUVPlane::UV, &output[2 * i + 1]);
        } else if (format == FRAMEWORK_FORMAT_YUV_420_888) {
            YUVConverter::createYUVGLTex(GL_TEXTURE0, width, height,
                                         format, YUVPlane::Y, &output[3 * i]);
            YUVConverter::createYUVGLTex(GL_TEXTURE1, width / 2, height / 2,
                                         format, YUVPlane::U, &output[3 * i + 1]);
            YUVConverter::createYUVGLTex(GL_TEXTURE2, width / 2, height / 2,
                                         format, YUVPlane::V, &output[3 * i + 2]);
        }
    }
}

void FrameBuffer::destroyYUVTextures(uint32_t type,
                                     uint32_t count,
                                     uint32_t* textures) {
    AutoLock mutex(m_lock);
    RecursiveScopedContextBind bind(getPbufferSurfaceContextHelper());
    if (type == FRAMEWORK_FORMAT_NV12) {
        s_gles2.glDeleteTextures(2 * count, textures);
    } else if (type == FRAMEWORK_FORMAT_YUV_420_888) {
        s_gles2.glDeleteTextures(3 * count, textures);
    }
}

extern "C" {
typedef void (*yuv_updater_t)(void* privData,
                              uint32_t type,
                              uint32_t* textures);
}

void FrameBuffer::updateYUVTextures(uint32_t type,
                                    uint32_t* textures,
                                    void* privData,
                                    void* func) {
    AutoLock mutex(m_lock);
    RecursiveScopedContextBind bind(getPbufferSurfaceContextHelper());

    yuv_updater_t updater = (yuv_updater_t)func;
    uint32_t gtextures[3] = {0, 0, 0};

    if (type == FRAMEWORK_FORMAT_NV12) {
        gtextures[0] = s_gles2.glGetGlobalTexName(textures[0]);
        gtextures[1] = s_gles2.glGetGlobalTexName(textures[1]);
    } else if (type == FRAMEWORK_FORMAT_YUV_420_888) {
        gtextures[0] = s_gles2.glGetGlobalTexName(textures[0]);
        gtextures[1] = s_gles2.glGetGlobalTexName(textures[1]);
        gtextures[2] = s_gles2.glGetGlobalTexName(textures[2]);
    }

    updater(privData, type, gtextures);
}

void FrameBuffer::swapTexturesAndUpdateColorBuffer(uint32_t p_colorbuffer,
                                                   int x,
                                                   int y,
                                                   int width,
                                                   int height,
                                                   uint32_t format,
                                                   uint32_t type,
                                                   uint32_t texture_type,
                                                   uint32_t* textures) {
    {
        AutoLock mutex(m_lock);
        ColorBufferPtr colorBuffer = findColorBuffer(p_colorbuffer);
        if (!colorBuffer) {
            // bad colorbuffer handle
            return;
        }
        colorBuffer->swapYUVTextures(texture_type, textures);
    }

    updateColorBuffer(p_colorbuffer, x, y, width, height, format, type,
                      nullptr);
}

bool FrameBuffer::updateBuffer(HandleType p_buffer, uint64_t offset, uint64_t size, void* bytes) {
    if (m_guestUsesAngle) {
        return goldfish_vk::updateBufferFromBytes(p_buffer, offset, size, bytes);
    }

    AutoLock mutex(m_lock);

    BufferPtr buffer = findBuffer(p_buffer);
    if (!buffer) {
        ERR("Failed to update buffer: buffer %d not found.", p_buffer);
        return false;
    }

    buffer->subUpdate(offset, size, bytes);

    return true;
}

bool FrameBuffer::updateColorBuffer(HandleType p_colorbuffer,
                                    int x,
                                    int y,
                                    int width,
                                    int height,
                                    GLenum format,
                                    GLenum type,
                                    void* pixels) {
    if (width == 0 || height == 0) {
        return false;
    }

    if (m_guestUsesAngle) {
        return goldfish_vk::updateColorBufferFromBytes(p_colorbuffer, x, y, width, height, pixels);
    }

    AutoLock mutex(m_lock);

    ColorBufferPtr colorBuffer = findColorBuffer(p_colorbuffer);
    if (!colorBuffer) {
        // bad colorbuffer handle
        return false;
    }

    colorBuffer->subUpdate(x, y, width, height, format, type, pixels);

    return true;
}

bool FrameBuffer::replaceColorBufferContents(
    HandleType p_colorbuffer, const void* pixels, size_t numBytes) {
    AutoLock mutex(m_lock);

    ColorBufferPtr colorBuffer = findColorBuffer(p_colorbuffer);
    if (!colorBuffer) {
        // bad colorbuffer handle
        return false;
    }

    return colorBuffer->replaceContents(pixels, numBytes);
}

bool FrameBuffer::readColorBufferContents(
    HandleType p_colorbuffer, size_t* numBytes, void* pixels) {

    AutoLock mutex(m_lock);

    ColorBufferPtr colorBuffer = findColorBuffer(p_colorbuffer);
    if (!colorBuffer) {
        // bad colorbuffer handle
        return false;
    }

    return colorBuffer->readContents(numBytes, pixels);
}

bool FrameBuffer::getColorBufferInfo(
    HandleType p_colorbuffer, int* width, int* height, GLint* internalformat,
    FrameworkFormat* frameworkFormat) {

    AutoLock mutex(m_lock);

    ColorBufferPtr colorBuffer = findColorBuffer(p_colorbuffer);
    if (!colorBuffer) {
        // bad colorbuffer handle
        return false;
    }

    *width = colorBuffer->getWidth();
    *height = colorBuffer->getHeight();
    *internalformat = colorBuffer->getInternalFormat();
    if (frameworkFormat) {
        *frameworkFormat = colorBuffer->getFrameworkFormat();
    }

    return true;
}

bool FrameBuffer::getBufferInfo(HandleType p_buffer, int* size) {
    AutoLock mutex(m_lock);

    BufferMap::iterator c(m_buffers.find(p_buffer));
    if (c == m_buffers.end()) {
        // Bad buffer handle.
        return false;
    }

    auto buf = (*c).second.buffer;
    *size = buf->getSize();
    return true;
}

bool FrameBuffer::bindColorBufferToTexture(HandleType p_colorbuffer) {
    AutoLock mutex(m_lock);

    ColorBufferPtr colorBuffer = findColorBuffer(p_colorbuffer);
    if (!colorBuffer) {
        // bad colorbuffer handle
        return false;
    }

    return colorBuffer->bindToTexture();
}

bool FrameBuffer::bindColorBufferToTexture2(HandleType p_colorbuffer) {
    AutoLock mutex(m_lock);

    ColorBufferPtr colorBuffer = findColorBuffer(p_colorbuffer);
    if (!colorBuffer) {
        // bad colorbuffer handle
        return false;
    }

    return colorBuffer->bindToTexture2();
}

bool FrameBuffer::bindColorBufferToRenderbuffer(HandleType p_colorbuffer) {
    AutoLock mutex(m_lock);

    ColorBufferPtr colorBuffer = findColorBuffer(p_colorbuffer);
    if (!colorBuffer) {
        // bad colorbuffer handle
        return false;
    }

    return colorBuffer->bindToRenderbuffer();
}

bool FrameBuffer::bindContext(HandleType p_context,
                              HandleType p_drawSurface,
                              HandleType p_readSurface) {
    if (m_shuttingDown) {
        return false;
    }

    AutoLock mutex(m_lock);

    EmulatedEglWindowSurfacePtr draw, read;
    EmulatedEglContextPtr ctx;

    //
    // if this is not an unbind operation - make sure all handles are good
    //
    if (p_context || p_drawSurface || p_readSurface) {
        ctx = getContext_locked(p_context);
        if (!ctx)
            return false;
        EmulatedEglWindowSurfaceMap::iterator w(m_windows.find(p_drawSurface));
        if (w == m_windows.end()) {
            // bad surface handle
            return false;
        }
        draw = (*w).second.first;

        if (p_readSurface != p_drawSurface) {
            EmulatedEglWindowSurfaceMap::iterator w(m_windows.find(p_readSurface));
            if (w == m_windows.end()) {
                // bad surface handle
                return false;
            }
            read = (*w).second.first;
        } else {
            read = draw;
        }
    } else {
        // if unbind operation, sweep color buffers
        sweepColorBuffersLocked();
    }

    if (!s_egl.eglMakeCurrent(getDisplay(),
                              draw ? draw->getEGLSurface() : EGL_NO_SURFACE,
                              read ? read->getEGLSurface() : EGL_NO_SURFACE,
                              ctx ? ctx->getEGLContext() : EGL_NO_CONTEXT)) {
        ERR("eglMakeCurrent failed");
        return false;
    }

    //
    // Bind the surface(s) to the context
    //
    RenderThreadInfoGl* const tinfo = RenderThreadInfoGl::get();
    if (!tinfo) {
        GFXSTREAM_ABORT(FatalError(ABORT_REASON_OTHER))
            << "Render thread GL not available.";
    }

    EmulatedEglWindowSurfacePtr bindDraw, bindRead;
    if (draw.get() == NULL && read.get() == NULL) {
        // Unbind the current read and draw surfaces from the context
        bindDraw = tinfo->currDrawSurf;
        bindRead = tinfo->currReadSurf;
    } else {
        bindDraw = draw;
        bindRead = read;
    }

    if (bindDraw.get() != NULL && bindRead.get() != NULL) {
        if (bindDraw.get() != bindRead.get()) {
            bindDraw->bind(ctx, EmulatedEglWindowSurface::BIND_DRAW);
            bindRead->bind(ctx, EmulatedEglWindowSurface::BIND_READ);
        } else {
            bindDraw->bind(ctx, EmulatedEglWindowSurface::BIND_READDRAW);
        }
    }

    //
    // update thread info with current bound context
    //
    tinfo->currContext = ctx;
    tinfo->currDrawSurf = draw;
    tinfo->currReadSurf = read;
    if (ctx) {
        if (ctx->clientVersion() > GLESApi_CM)
            tinfo->m_gl2Dec.setContextData(&ctx->decoderContextData());
        else
            tinfo->m_glDec.setContextData(&ctx->decoderContextData());
    } else {
        tinfo->m_glDec.setContextData(NULL);
        tinfo->m_gl2Dec.setContextData(NULL);
    }
    return true;
}

EmulatedEglContextPtr FrameBuffer::getContext_locked(HandleType p_context) {
    return android::base::findOrDefault(m_contexts, p_context);
}

EmulatedEglWindowSurfacePtr FrameBuffer::getWindowSurface_locked(HandleType p_windowsurface) {
    return android::base::findOrDefault(m_windows, p_windowsurface).first;
}

HandleType FrameBuffer::createClientImage(HandleType context,
                                          EGLenum target,
                                          GLuint buffer) {
    EGLContext eglContext = EGL_NO_CONTEXT;
    if (context) {
        AutoLock mutex(m_lock);
        EmulatedEglContextMap::const_iterator rcIt = m_contexts.find(context);
        if (rcIt == m_contexts.end()) {
            // bad context handle
            return false;
        }
        eglContext =
                rcIt->second ? rcIt->second->getEGLContext() : EGL_NO_CONTEXT;
    }

    EGLImageKHR image = s_egl.eglCreateImageKHR(
            getDisplay(), eglContext, target,
            reinterpret_cast<EGLClientBuffer>(buffer), NULL);
    HandleType imgHnd = (HandleType) reinterpret_cast<uintptr_t>(image);

    RenderThreadInfo* tInfo = RenderThreadInfo::get();
    uint64_t puid = tInfo->m_puid;
    if (puid) {
        AutoLock mutex(m_lock);
        m_procOwnedEGLImages[puid].insert(imgHnd);
    }
    return imgHnd;
}

EGLBoolean FrameBuffer::destroyClientImage(HandleType image) {
    // eglDestroyImageKHR has its own lock  already.
    EGLBoolean ret = s_egl.eglDestroyImageKHR(
            getDisplay(), reinterpret_cast<EGLImageKHR>(image));
    if (!ret)
        return false;
    RenderThreadInfo* tInfo = RenderThreadInfo::get();
    uint64_t puid = tInfo->m_puid;
    if (puid) {
        AutoLock mutex(m_lock);
        m_procOwnedEGLImages[puid].erase(image);
        // We don't explicitly call m_procOwnedEGLImages.erase(puid) when the
        // size reaches 0, since it could go between zero and one many times in
        // the lifetime of a process. It will be cleaned up by
        // cleanupProcGLObjects(puid) when the process is dead.
    }
    return true;
}

void FrameBuffer::createTrivialContext(HandleType shared,
                                       HandleType* contextOut,
                                       HandleType* surfOut) {
    assert(contextOut);
    assert(surfOut);

    *contextOut = createEmulatedEglContext(0, shared, GLESApi_2);
    // Zero size is formally allowed here, but SwiftShader doesn't like it and
    // fails.
    *surfOut = createEmulatedEglWindowSurface(0, 1, 1);
}

void FrameBuffer::createSharedTrivialContext(EGLContext* contextOut,
                                             EGLSurface* surfOut) {
    assert(contextOut);
    assert(surfOut);

    const EmulatedEglConfig* config = getConfigs()->get(0 /* p_config */);
    if (!config) return;

    int maj, min;
    emugl::getGlesVersion(&maj, &min);

    const EGLint contextAttribs[] = {
        EGL_CONTEXT_MAJOR_VERSION_KHR, maj,
        EGL_CONTEXT_MINOR_VERSION_KHR, min,
        EGL_NONE };

    *contextOut = s_egl.eglCreateContext(
            getDisplay(), config->getHostEglConfig(), getGlobalEGLContext(), contextAttribs);

    const EGLint pbufAttribs[] = {
        EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };

    *surfOut = s_egl.eglCreatePbufferSurface(getDisplay(), config->getHostEglConfig(), pbufAttribs);
}

void FrameBuffer::destroySharedTrivialContext(EGLContext context,
                                              EGLSurface surface) {
    if (getDisplay() != EGL_NO_DISPLAY) {
        s_egl.eglDestroyContext(getDisplay(), context);
        s_egl.eglDestroySurface(getDisplay(), surface);
    }
}

bool FrameBuffer::post(HandleType p_colorbuffer, bool needLockAndBind) {
    if (m_guestUsesAngle) {
        goldfish_vk::updateColorBufferFromGl(p_colorbuffer);
    }

    auto res = postImplSync(p_colorbuffer, needLockAndBind);
    if (res) setGuestPostedAFrame();
    return res;
}

void FrameBuffer::postWithCallback(HandleType p_colorbuffer, Post::CompletionCallback callback,
                                   bool needLockAndBind) {
    if (m_guestUsesAngle) {
        goldfish_vk::updateColorBufferFromGl(p_colorbuffer);
    }

    AsyncResult res = postImpl(p_colorbuffer, callback, needLockAndBind);
    if (res.Succeeded()) {
        setGuestPostedAFrame();
    }

    if (!res.CallbackScheduledOrFired()) {
        // If postImpl fails, we have not fired the callback. postWithCallback
        // should always ensure the callback fires.
        std::shared_future<void> callbackRes = std::async(std::launch::deferred, [] {});
        callback(callbackRes);
    }
}

bool FrameBuffer::postImplSync(HandleType p_colorbuffer,
    bool needLockAndBind,
    bool repaint) {
    std::promise<void> promise;
    std::future<void> completeFuture = promise.get_future();
    auto posted = postImpl(
        p_colorbuffer,
        [&](std::shared_future<void> waitForGpu) {
            waitForGpu.wait();
            promise.set_value();
        },
        needLockAndBind, repaint);
    if (posted.CallbackScheduledOrFired()) {
        completeFuture.wait();
    }

    return posted.Succeeded();
}

AsyncResult FrameBuffer::postImpl(HandleType p_colorbuffer,
                           Post::CompletionCallback callback,
                           bool needLockAndBind,
                           bool repaint) {
    if (needLockAndBind) {
        m_lock.lock();
    }
    AsyncResult ret = AsyncResult::FAIL_AND_CALLBACK_NOT_SCHEDULED;

    ColorBufferPtr colorBuffer = nullptr;
    {
        AutoLock colorBufferMapLock(m_colorBufferMapLock);
        ColorBufferMap::iterator c = m_colorbuffers.find(p_colorbuffer);
        if (c != m_colorbuffers.end()) {
            colorBuffer = c->second.cb;
            markOpened(&c->second);
        }
    }
    if (!colorBuffer) {
        goto EXIT;
    }

    m_lastPostedColorBuffer = p_colorbuffer;

    colorBuffer->touch();
    if (m_subWin) {
        Post postCmd;
        postCmd.cmd = PostCmd::Post;
        postCmd.cb = colorBuffer.get();
        postCmd.completionCallback = std::make_unique<Post::CompletionCallback>(callback);
        sendPostWorkerCmd(std::move(postCmd));
        ret = AsyncResult::OK_AND_CALLBACK_SCHEDULED;
    } else {
        // If there is no sub-window, don't display anything, the client will
        // rely on m_onPost to get the pixels instead.
        ret = AsyncResult::OK_AND_CALLBACK_NOT_SCHEDULED;
    }

    //
    // output FPS and performance usage statistics
    //
    if (m_fpsStats) {
        long long currTime = android::base::getHighResTimeUs() / 1000;
        m_statsNumFrames++;
        if (currTime - m_statsStartTime >= 1000) {
            if (m_fpsStats) {
                float dt = (float)(currTime - m_statsStartTime) / 1000.0f;
                printf("FPS: %5.3f \n", (float)m_statsNumFrames / dt);
                m_statsNumFrames = 0;
            }
            m_statsStartTime = currTime;
        }
    }

    //
    // Send framebuffer (without FPS overlay) to callback
    //
    if (m_onPost.size() == 0) {
        goto EXIT;
    }
    for (auto& iter : m_onPost) {
        ColorBufferPtr cb;
        if (iter.first == 0) {
            cb = colorBuffer;
        } else {
            uint32_t colorBuffer;
            if (getDisplayColorBuffer(iter.first, &colorBuffer) < 0) {
                ERR("Failed to get color buffer for display %d, skip onPost", iter.first);
                continue;
            }

            cb = findColorBuffer(colorBuffer);
            if (!cb) {
                ERR("Failed to find colorbuffer %d, skip onPost", colorBuffer);
                continue;
            }
        }

        if (asyncReadbackSupported()) {
            ensureReadbackWorker();
            m_readbackWorker->doNextReadback(iter.first, cb.get(), iter.second.img,
                repaint, iter.second.readBgra);
        } else {
            cb->readback(iter.second.img, iter.second.readBgra);
            doPostCallback(iter.second.img, iter.first);
        }
    }

EXIT:
    if (needLockAndBind) {
        m_lock.unlock();
    }
    return ret;
}

void FrameBuffer::doPostCallback(void* pixels, uint32_t displayId) {
    const auto& iter = m_onPost.find(displayId);
    if (iter == m_onPost.end()) {
        ERR("Cannot find post callback function for display %d", displayId);
        return;
    }
    iter->second.cb(iter->second.context, displayId, iter->second.width,
                    iter->second.height, -1, GL_RGBA, GL_UNSIGNED_BYTE,
                    (unsigned char*)pixels);
}

void FrameBuffer::getPixels(void* pixels, uint32_t bytes, uint32_t displayId) {
    const auto& iter = m_onPost.find(displayId);
    if (iter == m_onPost.end()) {
        ERR("Display %d not configured for recording yet", displayId);
        return;
    }
    std::future<void> completeFuture = m_readbackThread.enqueue(
        {ReadbackCmd::GetPixels, displayId, 0, pixels, bytes});
    completeFuture.wait();
}

void FrameBuffer::flushReadPipeline(int displayId) {
    const auto& iter = m_onPost.find(displayId);
    if (iter == m_onPost.end()) {
        ERR("Cannot find onPost pixels for display %d", displayId);
        return;
    }

    ensureReadbackWorker();
    m_readbackWorker->flushPipeline(displayId);
}

void FrameBuffer::ensureReadbackWorker() {
    if (!m_readbackWorker) m_readbackWorker.reset(new ReadbackWorker);
}

static void sFrameBuffer_ReadPixelsCallback(
    void* pixels, uint32_t bytes, uint32_t displayId) {
    FrameBuffer::getFB()->getPixels(pixels, bytes, displayId);
}

static void sFrameBuffer_FlushReadPixelPipeline(int displayId) {
    FrameBuffer::getFB()->flushReadPipeline(displayId);
}

bool FrameBuffer::asyncReadbackSupported() {
    return m_emulationGl && m_emulationGl->isAsyncReadbackSupported();
}

emugl::Renderer::ReadPixelsCallback
FrameBuffer::getReadPixelsCallback() {
    return sFrameBuffer_ReadPixelsCallback;
}

emugl::Renderer::FlushReadPixelPipeline FrameBuffer::getFlushReadPixelPipeline() {
    return sFrameBuffer_FlushReadPixelPipeline;
}

bool FrameBuffer::repost(bool needLockAndBind) {
    GL_LOG("Reposting framebuffer.");
    if (m_displayVk) {
        return true;
    }
    if (m_lastPostedColorBuffer &&
        sInitialized.load(std::memory_order_relaxed)) {
        GL_LOG("Has last posted colorbuffer and is initialized; post.");
        return postImplSync(m_lastPostedColorBuffer, needLockAndBind, true);
    } else {
        GL_LOG("No repost: no last posted color buffer");
        if (!sInitialized.load(std::memory_order_relaxed)) {
            GL_LOG("No repost: initialization is not finished.");
        }
    }
    return false;
}

template <class Collection>
static void saveProcOwnedCollection(Stream* stream, const Collection& c) {
    // Exclude empty handle lists from saving as they add no value but only
    // increase the snapshot size; keep the format compatible with
    // android::base::saveCollection() though.
    const int count =
            std::count_if(c.begin(), c.end(),
                          [](const typename Collection::value_type& pair) {
                              return !pair.second.empty();
                          });
    stream->putBe32(count);
    for (const auto& pair : c) {
        if (pair.second.empty()) {
            continue;
        }
        stream->putBe64(pair.first);
        saveCollection(stream, pair.second,
                       [](Stream* s, HandleType h) { s->putBe32(h); });
    }
}

template <class Collection>
static void loadProcOwnedCollection(Stream* stream, Collection* c) {
    loadCollection(stream, c,
                   [](Stream* stream) -> typename Collection::value_type {
        const int processId = stream->getBe64();
        typename Collection::mapped_type handles;
        loadCollection(stream, &handles, [](Stream* s) { return s->getBe32(); });
        return { processId, std::move(handles) };
    });
}

void FrameBuffer::getScreenshot(unsigned int nChannels, unsigned int* width,
        unsigned int* height, std::vector<unsigned char>& pixels, int displayId,
        int desiredWidth, int desiredHeight, int desiredRotation) {
    AutoLock mutex(m_lock);
    uint32_t w, h, cb;
    if (!emugl::get_emugl_multi_display_operations().getMultiDisplay(displayId,
                                                                     nullptr,
                                                                     nullptr,
                                                                     &w,
                                                                     &h,
                                                                     nullptr,
                                                                     nullptr,
                                                                     nullptr)) {
        ERR("Screenshot of invalid display %d", displayId);
        *width = 0;
        *height = 0;
        pixels.resize(0);
        return;
    }
    if (nChannels != 3 && nChannels != 4) {
        ERR("Screenshot only support 3(RGB) or 4(RGBA) channels");
        *width = 0;
        *height = 0;
        pixels.resize(0);
        return;
    }
    emugl::get_emugl_multi_display_operations().getDisplayColorBuffer(displayId, &cb);
    if (displayId == 0) {
        cb = m_lastPostedColorBuffer;
    }
    ColorBufferPtr colorBuffer = findColorBuffer(cb);
    if (!colorBuffer) {
        *width = 0;
        *height = 0;
        pixels.resize(0);
        return;
    }

    *width = (desiredWidth == 0) ? w : desiredWidth;
    *height = (desiredHeight == 0) ? h : desiredHeight;
    if (desiredRotation == SKIN_ROTATION_90 || desiredRotation == SKIN_ROTATION_270) {
        std::swap(*width, *height);
    }
    pixels.resize(nChannels * (*width) * (*height));

    GLenum format = nChannels == 3 ? GL_RGB : GL_RGBA;

    Post scrCmd;
    scrCmd.cmd = PostCmd::Screenshot;
    scrCmd.screenshot.cb = colorBuffer.get();
    scrCmd.screenshot.screenwidth = *width;
    scrCmd.screenshot.screenheight = *height;
    scrCmd.screenshot.format = format;
    scrCmd.screenshot.type = GL_UNSIGNED_BYTE;
    scrCmd.screenshot.rotation = desiredRotation;
    scrCmd.screenshot.pixels = pixels.data();

    std::future<void> completeFuture = sendPostWorkerCmd(std::move(scrCmd));
    completeFuture.wait();
}

void FrameBuffer::onLastColorBufferRef(uint32_t handle) {
    if (!mOutstandingColorBufferDestroys.trySend((HandleType)handle)) {
        ERR("warning: too many outstanding "
            "color buffer destroys. leaking handle 0x%x",
            handle);
    }
}

bool FrameBuffer::decColorBufferRefCountLocked(HandleType p_colorbuffer) {
    AutoLock colorBufferMapLock(m_colorBufferMapLock);
    const auto& it = m_colorbuffers.find(p_colorbuffer);
    if (it != m_colorbuffers.end()) {
        it->second.refcount -= 1;
        if (it->second.refcount == 0) {
            m_colorbuffers.erase(p_colorbuffer);
            return true;
        }
    }
    return false;
}

bool FrameBuffer::compose(uint32_t bufferSize, void* buffer, bool needPost) {
    std::promise<void> promise;
    std::future<void> completeFuture = promise.get_future();
    auto composeRes = composeWithCallback(
        bufferSize, buffer, [&](std::shared_future<void> waitForGpu) {
            waitForGpu.wait();
            promise.set_value();
        });
    if (!composeRes.Succeeded()) {
        return false;
    }

    if (composeRes.CallbackScheduledOrFired()) {
        completeFuture.wait();
    }

    if (needPost) {
        // AEMU with -no-window mode uses this code path.
        ComposeDevice* composeDevice = (ComposeDevice*)buffer;
        AutoLock mutex(m_lock);

        switch (composeDevice->version) {
            case 1: {
                RecursiveScopedContextBind scopedBind(getPbufferSurfaceContextHelper());
                post(composeDevice->targetHandle, false);
                break;
            }
            case 2: {
                ComposeDevice_v2* composeDeviceV2 = (ComposeDevice_v2*)buffer;
                if (composeDeviceV2->displayId == 0) {
                    RecursiveScopedContextBind scopedBind(getPbufferSurfaceContextHelper());
                    post(composeDeviceV2->targetHandle, false);
                }
                break;
            }
            default: {
                return false;
            }
        }
    }
    return true;
}

AsyncResult FrameBuffer::composeWithCallback(uint32_t bufferSize, void* buffer,
                                      Post::CompletionCallback callback) {
    ComposeDevice* p = (ComposeDevice*)buffer;
    AutoLock mutex(m_lock);

    switch (p->version) {
    case 1: {
        Post composeCmd;
        composeCmd.composeVersion = 1;
        composeCmd.composeBuffer.resize(bufferSize);
        memcpy(composeCmd.composeBuffer.data(), buffer, bufferSize);
        composeCmd.completionCallback = std::make_unique<Post::CompletionCallback>(callback);
        composeCmd.cmd = PostCmd::Compose;
        sendPostWorkerCmd(std::move(composeCmd));
        return AsyncResult::OK_AND_CALLBACK_SCHEDULED;
    }

    case 2: {
        // support for multi-display
        ComposeDevice_v2* p2 = (ComposeDevice_v2*)buffer;
        if (p2->displayId != 0) {
            mutex.unlock();
            setDisplayColorBuffer(p2->displayId, p2->targetHandle);
            mutex.lock();
        }
        Post composeCmd;
        composeCmd.composeVersion = 2;
        composeCmd.composeBuffer.resize(bufferSize);
        memcpy(composeCmd.composeBuffer.data(), buffer, bufferSize);
        composeCmd.completionCallback = std::make_unique<Post::CompletionCallback>(callback);
        composeCmd.cmd = PostCmd::Compose;
        sendPostWorkerCmd(std::move(composeCmd));
        return AsyncResult::OK_AND_CALLBACK_SCHEDULED;
    }

    default:
       ERR("yet to handle composition device version: %d", p->version);
        return AsyncResult::FAIL_AND_CALLBACK_NOT_SCHEDULED;
    }
}

void FrameBuffer::onSave(Stream* stream,
                         const android::snapshot::ITextureSaverPtr& textureSaver) {
    // Things we do not need to snapshot:
    //     m_eglSurface
    //     m_eglContext
    //     m_pbufSurface
    //     m_pbufContext
    //     m_prevContext
    //     m_prevReadSurf
    //     m_prevDrawSurf
    AutoLock mutex(m_lock);
    // set up a context because some snapshot commands try using GL
    RecursiveScopedContextBind scopedBind(getPbufferSurfaceContextHelper());
    // eglPreSaveContext labels all guest context textures to be saved
    // (textures created by the host are not saved!)
    // eglSaveAllImages labels all EGLImages (both host and guest) to be saved
    // and save all labeled textures and EGLImages.
    if (s_egl.eglPreSaveContext && s_egl.eglSaveAllImages) {
        for (const auto& ctx : m_contexts) {
            s_egl.eglPreSaveContext(getDisplay(), ctx.second->getEGLContext(),
                    stream);
        }
        s_egl.eglSaveAllImages(getDisplay(), stream, &textureSaver);
    }
    // Don't save subWindow's x/y/w/h here - those are related to the current
    // emulator UI state, not guest state that we're saving.
    stream->putBe32(m_framebufferWidth);
    stream->putBe32(m_framebufferHeight);
    stream->putFloat(m_dpr);

    stream->putBe32(m_useSubWindow);
    stream->putBe32(/*Obsolete m_eglContextInitialized =*/1);

    stream->putBe32(m_fpsStats);
    stream->putBe32(m_statsNumFrames);
    stream->putBe64(m_statsStartTime);

    // Save all contexts.
    // Note: some of the contexts might not be restored yet. In such situation
    // we skip reading from GPU (for non-texture objects) or force a restore in
    // previous eglPreSaveContext and eglSaveAllImages calls (for texture
    // objects).
    // TODO: skip reading from GPU even for texture objects.
    saveCollection(stream, m_contexts,
                   [](Stream* s, const EmulatedEglContextMap::value_type& pair) {
        pair.second->onSave(s);
    });

    // We don't need to save |m_colorBufferCloseTsMap| here - there's enough
    // information to reconstruct it when loading.
    uint64_t now = android::base::getUnixTimeUs();

    {
        AutoLock colorBufferMapLock(m_colorBufferMapLock);
        stream->putByte(m_guestManagedColorBufferLifetime);
        saveCollection(stream, m_colorbuffers,
                       [now](Stream* s, const ColorBufferMap::value_type& pair) {
                           pair.second.cb->onSave(s);
                           s->putBe32(pair.second.refcount);
                           s->putByte(pair.second.opened);
                           s->putBe32(std::max<uint64_t>(0, now - pair.second.closedTs));
                       });
    }
    stream->putBe32(m_lastPostedColorBuffer);
    saveCollection(stream, m_windows,
                   [](Stream* s, const EmulatedEglWindowSurfaceMap::value_type& pair) {
        pair.second.first->onSave(s);
        s->putBe32(pair.second.second); // Color buffer handle.
    });

    saveProcOwnedCollection(stream, m_procOwnedEmulatedEglWindowSurfaces);
    saveProcOwnedCollection(stream, m_procOwnedColorBuffers);
    saveProcOwnedCollection(stream, m_procOwnedEGLImages);
    saveProcOwnedCollection(stream, m_procOwnedEmulatedEglContexts);

    // Save Vulkan state
    if (feature_is_enabled(kFeature_VulkanSnapshots) &&
        goldfish_vk::VkDecoderGlobalState::get()) {
        goldfish_vk::VkDecoderGlobalState::get()->save(stream);
    }

    if (s_egl.eglPostSaveContext) {
        for (const auto& ctx : m_contexts) {
            s_egl.eglPostSaveContext(getDisplay(), ctx.second->getEGLContext(),
                    stream);
        }
        // We need to run the post save step for m_eglContext
        // to mark their texture handles dirty
        if (getContext() != EGL_NO_CONTEXT) {
            s_egl.eglPostSaveContext(getDisplay(), getContext(), stream);
        }
    }

}

bool FrameBuffer::onLoad(Stream* stream,
                         const android::snapshot::ITextureLoaderPtr& textureLoader) {
    AutoLock lock(m_lock);
    // cleanups
    {
        sweepColorBuffersLocked();

        RecursiveScopedContextBind scopedBind(getPbufferSurfaceContextHelper());
        bool cleanupComplete = false;
        {
            AutoLock colorBufferMapLock(m_colorBufferMapLock);
            if (m_procOwnedEmulatedEglWindowSurfaces.empty() && m_procOwnedColorBuffers.empty() &&
                m_procOwnedEGLImages.empty() && m_procOwnedEmulatedEglContexts.empty() &&
                m_procOwnedCleanupCallbacks.empty() &&
                (!m_contexts.empty() || !m_windows.empty() ||
                 m_colorbuffers.size() > m_colorBufferDelayedCloseList.size())) {
                // we are likely on a legacy system image, which does not have
                // process owned objects. We need to force cleanup everything
                m_contexts.clear();
                m_windows.clear();
                m_colorbuffers.clear();
                cleanupComplete = true;
            }
        }
        if (!cleanupComplete) {
            std::vector<HandleType> colorBuffersToCleanup;

            while (m_procOwnedEmulatedEglWindowSurfaces.size()) {
                auto cleanupHandles = cleanupProcGLObjects_locked(
                        m_procOwnedEmulatedEglWindowSurfaces.begin()->first, true);
                colorBuffersToCleanup.insert(colorBuffersToCleanup.end(),
                    cleanupHandles.begin(), cleanupHandles.end());
            }
            while (m_procOwnedColorBuffers.size()) {
                auto cleanupHandles = cleanupProcGLObjects_locked(
                        m_procOwnedColorBuffers.begin()->first, true);
                colorBuffersToCleanup.insert(colorBuffersToCleanup.end(),
                    cleanupHandles.begin(), cleanupHandles.end());
            }
            while (m_procOwnedEGLImages.size()) {
                auto cleanupHandles = cleanupProcGLObjects_locked(
                        m_procOwnedEGLImages.begin()->first, true);
                colorBuffersToCleanup.insert(colorBuffersToCleanup.end(),
                    cleanupHandles.begin(), cleanupHandles.end());
            }
            while (m_procOwnedEmulatedEglContexts.size()) {
                auto cleanupHandles = cleanupProcGLObjects_locked(
                        m_procOwnedEmulatedEglContexts.begin()->first, true);
                colorBuffersToCleanup.insert(colorBuffersToCleanup.end(),
                    cleanupHandles.begin(), cleanupHandles.end());
            }

            std::vector<std::function<void()>> cleanupCallbacks;

            while (m_procOwnedCleanupCallbacks.size()) {
                auto it = m_procOwnedCleanupCallbacks.begin();
                while (it != m_procOwnedCleanupCallbacks.end()) {
                    for (auto it2 : it->second) {
                        cleanupCallbacks.push_back(it2.second);
                    }
                    it = m_procOwnedCleanupCallbacks.erase(it);
                }
            }

            m_procOwnedResources.clear();

            performDelayedColorBufferCloseLocked(true);

            lock.unlock();

            for (auto colorBufferHandle : colorBuffersToCleanup) {
                goldfish_vk::teardownVkColorBuffer(colorBufferHandle);
            }

            for (auto cb : cleanupCallbacks) {
                cb();
            }

            lock.lock();
            cleanupComplete = true;
        }
        m_colorBufferDelayedCloseList.clear();
        assert(m_contexts.empty());
        assert(m_windows.empty());
        {
            AutoLock colorBufferMapLock(m_colorBufferMapLock);
            if (!m_colorbuffers.empty()) {
                ERR("warning: on load, stale colorbuffers: %zu", m_colorbuffers.size());
                m_colorbuffers.clear();
            }
            assert(m_colorbuffers.empty());
        }
#ifdef SNAPSHOT_PROFILE
        uint64_t texTime = android::base::getUnixTimeUs();
#endif
        if (s_egl.eglLoadAllImages) {
            s_egl.eglLoadAllImages(getDisplay(), stream, &textureLoader);
        }
#ifdef SNAPSHOT_PROFILE
        printf("Texture load time: %lld ms\n",
               (long long)(android::base::getUnixTimeUs() - texTime) / 1000);
#endif
    }
    // See comment about subwindow position in onSave().
    m_framebufferWidth = stream->getBe32();
    m_framebufferHeight = stream->getBe32();
    m_dpr = stream->getFloat();
    // TODO: resize the window
    //
    m_useSubWindow = stream->getBe32();
    /*Obsolete m_eglContextInitialized =*/stream->getBe32();

    m_fpsStats = stream->getBe32();
    m_statsNumFrames = stream->getBe32();
    m_statsStartTime = stream->getBe64();

    loadCollection(stream, &m_contexts,
                   [this](Stream* stream) -> EmulatedEglContextMap::value_type {
                        if (!m_emulationGl) {
                            GFXSTREAM_ABORT(FatalError(ABORT_REASON_OTHER))
                                << "GL/EGL emulation not enabled.";
                        }

                        auto context = m_emulationGl->loadEmulatedEglContext(stream);
                        auto contextHandle = context ? context->getHndl() : 0;
                        return { contextHandle, std::move(context) };
                   });
    assert(!android::base::find(m_contexts, 0));

    auto now = android::base::getUnixTimeUs();
    {
        AutoLock colorBufferMapLock(m_colorBufferMapLock);
        m_guestManagedColorBufferLifetime = stream->getByte();
        loadCollection(
            stream, &m_colorbuffers, [this, now](Stream* stream) -> ColorBufferMap::value_type {
                ColorBufferPtr cb(ColorBuffer::onLoad(stream, getDisplay(), getPbufferSurfaceContextHelper(),
                                                      getTextureDraw(), isFastBlitSupported()));
                const HandleType handle = cb->getHndl();
                const unsigned refCount = stream->getBe32();
                const bool opened = stream->getByte();
                const uint64_t closedTs = now - stream->getBe32();
                if (refCount == 0) {
                    m_colorBufferDelayedCloseList.push_back({closedTs, handle});
                }
                return {handle, ColorBufferRef{std::move(cb), refCount, opened, closedTs}};
            });
    }
    m_lastPostedColorBuffer = static_cast<HandleType>(stream->getBe32());
    GL_LOG("Got lasted posted color buffer from snapshot");

    {
        AutoLock colorBufferMapLock(m_colorBufferMapLock);
        loadCollection(stream, &m_windows,
                       [this](Stream* stream) -> EmulatedEglWindowSurfaceMap::value_type {
                            if (!m_emulationGl) {
                                GFXSTREAM_ABORT(FatalError(ABORT_REASON_OTHER))
                                    << "GL/EGL emulation not enabled.";
                            }

                            auto window = m_emulationGl->loadEmulatedEglWindowSurface(
                                stream,
                                m_colorbuffers,
                                m_contexts);

                            HandleType handle = window->getHndl();
                            HandleType colorBufferHandle = stream->getBe32();
                            return { handle, { std::move(window), colorBufferHandle } };
                        });
    }

    loadProcOwnedCollection(stream, &m_procOwnedEmulatedEglWindowSurfaces);
    loadProcOwnedCollection(stream, &m_procOwnedColorBuffers);
    loadProcOwnedCollection(stream, &m_procOwnedEGLImages);
    loadProcOwnedCollection(stream, &m_procOwnedEmulatedEglContexts);

    if (s_egl.eglPostLoadAllImages) {
        s_egl.eglPostLoadAllImages(getDisplay(), stream);
    }

    registerTriggerWait();

    {
        RecursiveScopedContextBind scopedBind(getPbufferSurfaceContextHelper());
        AutoLock colorBufferMapLock(m_colorBufferMapLock);
        for (auto& it : m_colorbuffers) {
            if (it.second.cb) {
                it.second.cb->touch();
            }
        }
    }

    // Restore Vulkan state
    if (feature_is_enabled(kFeature_VulkanSnapshots) &&
        goldfish_vk::VkDecoderGlobalState::get()) {

        lock.unlock();
        GfxApiLogger gfxLogger;
        goldfish_vk::VkDecoderGlobalState::get()->load(stream, gfxLogger, m_healthMonitor);
        lock.lock();

    }

    repost(false);
    return true;
    // TODO: restore memory management
}

void FrameBuffer::lock() {
    m_lock.lock();
}

void FrameBuffer::unlock() {
    m_lock.unlock();
}

GLESDispatchMaxVersion FrameBuffer::getMaxGLESVersion() {
    if (!m_emulationGl) {
        return GLES_DISPATCH_MAX_VERSION_2;
    }
    return m_emulationGl->getGlesMaxDispatchVersion();
}

ColorBufferPtr FrameBuffer::findColorBuffer(HandleType p_colorbuffer) {
    AutoLock colorBufferMapLock(m_colorBufferMapLock);
    ColorBufferMap::iterator c(m_colorbuffers.find(p_colorbuffer));
    if (c == m_colorbuffers.end()) {
        return nullptr;
    }
    else {
        return c->second.cb;
    }
}

BufferPtr FrameBuffer::findBuffer(HandleType p_buffer) {
    AutoLock colorBufferMapLock(m_colorBufferMapLock);
    BufferMap::iterator b(m_buffers.find(p_buffer));
    if (b == m_buffers.end()) {
        return nullptr;
    } else {
        return b->second.buffer;
    }
}

void FrameBuffer::registerProcessCleanupCallback(void* key, std::function<void()> cb) {
    AutoLock mutex(m_lock);
    RenderThreadInfo* tInfo = RenderThreadInfo::get();
    if (!tInfo) return;

    auto& callbackMap = m_procOwnedCleanupCallbacks[tInfo->m_puid];
    callbackMap[key] = cb;
}

void FrameBuffer::unregisterProcessCleanupCallback(void* key) {
    AutoLock mutex(m_lock);
    RenderThreadInfo* tInfo = RenderThreadInfo::get();
    if (!tInfo) return;

    auto& callbackMap = m_procOwnedCleanupCallbacks[tInfo->m_puid];
    if (callbackMap.find(key) == callbackMap.end()) {
        ERR("warning: tried to erase nonexistent key %p "
            "associated with process %llu",
            key, (unsigned long long)(tInfo->m_puid));
    }
    callbackMap.erase(key);
}

const ProcessResources* FrameBuffer::getProcessResources(uint64_t puid) {
    AutoLock mutex(m_lock);
    auto i = m_procOwnedResources.find(puid);
    if (i == m_procOwnedResources.end()) {
        ERR("Failed to find process owned resources for puid %" PRIu64 ".", puid);
        return nullptr;
    }
    return i->second.get();
}

int FrameBuffer::createDisplay(uint32_t *displayId) {
    return emugl::get_emugl_multi_display_operations().createDisplay(displayId);
}

int FrameBuffer::createDisplay(uint32_t displayId) {
    return emugl::get_emugl_multi_display_operations().createDisplay(&displayId);
}

int FrameBuffer::destroyDisplay(uint32_t displayId) {
    return emugl::get_emugl_multi_display_operations().destroyDisplay(displayId);
}

int FrameBuffer::setDisplayColorBuffer(uint32_t displayId, uint32_t colorBuffer) {
    return emugl::get_emugl_multi_display_operations().
        setDisplayColorBuffer(displayId, colorBuffer);
}

int FrameBuffer::getDisplayColorBuffer(uint32_t displayId, uint32_t* colorBuffer) {
    return emugl::get_emugl_multi_display_operations().
        getDisplayColorBuffer(displayId, colorBuffer);
}

int FrameBuffer::getColorBufferDisplay(uint32_t colorBuffer, uint32_t* displayId) {
    return emugl::get_emugl_multi_display_operations().
        getColorBufferDisplay(colorBuffer, displayId);
}

int FrameBuffer::getDisplayPose(uint32_t displayId,
                                int32_t* x,
                                int32_t* y,
                                uint32_t* w,
                                uint32_t* h) {
    return emugl::get_emugl_multi_display_operations().
        getDisplayPose(displayId, x, y, w, h);
}

int FrameBuffer::setDisplayPose(uint32_t displayId,
                                int32_t x,
                                int32_t y,
                                uint32_t w,
                                uint32_t h,
                                uint32_t dpi) {
    return emugl::get_emugl_multi_display_operations().
        setDisplayPose(displayId, x, y, w, h, dpi);
}

void FrameBuffer::sweepColorBuffersLocked() {
    HandleType handleToDestroy;
    while (mOutstandingColorBufferDestroys.tryReceive(&handleToDestroy)) {
        bool needCleanup = decColorBufferRefCountLocked(handleToDestroy);
        if (needCleanup) {
            m_lock.unlock();
            goldfish_vk::teardownVkColorBuffer(handleToDestroy);
            m_lock.lock();
        }
    }
}

std::future<void> FrameBuffer::blockPostWorker(std::future<void> continueSignal) {
    std::promise<void> scheduled;
    std::future<void> scheduledFuture = scheduled.get_future();
    Post postCmd = {
        .cmd = PostCmd::Block,
        .block = std::make_unique<Post::Block>(Post::Block{
            .scheduledSignal = std::move(scheduled),
            .continueSignal = std::move(continueSignal),
        }),
    };
    sendPostWorkerCmd(std::move(postCmd));
    return scheduledFuture;
}

void FrameBuffer::waitForGpu(uint64_t eglsync) {
    FenceSync* fenceSync = FenceSync::getFromHandle(eglsync);

    if (!fenceSync) {
        ERR("err: fence sync 0x%llx not found", (unsigned long long)eglsync);
        return;
    }

    SyncThread::get()->triggerBlockedWaitNoTimeline(fenceSync);
}

void FrameBuffer::waitForGpuVulkan(uint64_t deviceHandle, uint64_t fenceHandle) {
    (void)deviceHandle;
    if (!m_emulationGl) {
        // Guest ANGLE should always use the asyncWaitForGpuVulkanWithCb call. FenceSync is a
        // wrapper over EGLSyncKHR and should not be used for pure Vulkan environment.
        return;
    }

    // Note: this will always be nullptr.
    FenceSync* fenceSync = FenceSync::getFromHandle(fenceHandle);

    // Note: This will always signal right away.
    SyncThread::get()->triggerBlockedWaitNoTimeline(fenceSync);
}

void FrameBuffer::asyncWaitForGpuWithCb(uint64_t eglsync, FenceCompletionCallback cb) {
    FenceSync* fenceSync = FenceSync::getFromHandle(eglsync);

    if (!fenceSync) {
        ERR("err: fence sync 0x%llx not found", (unsigned long long)eglsync);
        return;
    }

    SyncThread::get()->triggerWaitWithCompletionCallback(fenceSync, std::move(cb));
}

void FrameBuffer::asyncWaitForGpuVulkanWithCb(uint64_t deviceHandle, uint64_t fenceHandle, FenceCompletionCallback cb) {
    (void)deviceHandle;
    SyncThread::get()->triggerWaitVkWithCompletionCallback((VkFence)fenceHandle, std::move(cb));
}

void FrameBuffer::asyncWaitForGpuVulkanQsriWithCb(uint64_t image, FenceCompletionCallback cb) {
    SyncThread::get()->triggerWaitVkQsriWithCompletionCallback((VkImage)image, std::move(cb));
}

void FrameBuffer::waitForGpuVulkanQsri(uint64_t image) {
    (void)image;
    // Signal immediately, because this was a sync wait and it's vulkan.
    SyncThread::get()->triggerBlockedWaitNoTimeline(nullptr);
}

void FrameBuffer::setGuestManagedColorBufferLifetime(bool guestManaged) {
    m_guestManagedColorBufferLifetime = guestManaged;
}

HealthMonitor<>& FrameBuffer::getHealthMonitor() { return m_healthMonitor; }

bool FrameBuffer::platformImportResource(uint32_t handle, uint32_t info, void* resource) {
    if (!resource) {
        ERR("Error: resource was null");
    }

    AutoLock mutex(m_lock);

    ColorBufferPtr colorBuffer = findColorBuffer(handle);
    if (!colorBuffer) {
        ERR("Error: resource %u not found as a ColorBuffer", handle);
        return false;
    }

    uint32_t type = (info & RESOURCE_TYPE_MASK);
    bool preserveContent = (info & RESOURCE_USE_PRESERVE);

    switch (type) {
        case RESOURCE_TYPE_EGL_NATIVE_PIXMAP:
            return colorBuffer->importEglNativePixmap(resource, preserveContent);
        case RESOURCE_TYPE_EGL_IMAGE:
            return colorBuffer->importEglImage(resource, preserveContent);
        default:
            ERR("Error: unsupported resource type: %u", type);
            return false;
    }

    return true;
}

void* FrameBuffer::platformCreateSharedEglContext(void) {
    AutoLock lock(m_lock);

    EGLContext context = 0;
    EGLSurface surface = 0;
    createSharedTrivialContext(&context, &surface);

    void* underlyingContext = s_egl.eglGetNativeContextANDROID(getDisplay(), context);
    if (!underlyingContext) {
        ERR("Error: Underlying egl backend could not produce a native EGL context.");
        return nullptr;
    }

    m_platformEglContexts[underlyingContext] = { context, surface };

    return underlyingContext;
}

bool FrameBuffer::platformDestroySharedEglContext(void* underlyingContext) {
    AutoLock lock(m_lock);

    auto it = m_platformEglContexts.find(underlyingContext);
    if (it == m_platformEglContexts.end()) {
        ERR("Error: Could not find underlying egl context %p (perhaps already destroyed?)", underlyingContext);
        return false;
    }

    destroySharedTrivialContext(it->second.context, it->second.surface);

    m_platformEglContexts.erase(it);

    return true;
}

std::unique_ptr<BorrowedImageInfo> FrameBuffer::borrowColorBufferForComposition(
    uint32_t colorBufferHandle, bool colorBufferIsTarget) {
    if (m_useVulkanComposition) {
        return goldfish_vk::borrowColorBufferForComposition(colorBufferHandle, colorBufferIsTarget);
    }

    ColorBufferPtr colorBufferPtr = findColorBuffer(colorBufferHandle);
    if (!colorBufferPtr) {
        ERR("Failed to get borrowed image info for ColorBuffer:%d", colorBufferHandle);
        return nullptr;
    }
    return colorBufferPtr->getBorrowedImageInfo();
}

std::unique_ptr<BorrowedImageInfo> FrameBuffer::borrowColorBufferForDisplay(
        uint32_t colorBufferHandle) {
    if (m_useVulkanComposition) {
        return goldfish_vk::borrowColorBufferForDisplay(colorBufferHandle);
    }

    ColorBufferPtr colorBufferPtr = findColorBuffer(colorBufferHandle);
    if (!colorBufferPtr) {
        ERR("Failed to get borrowed image info for ColorBuffer:%d", colorBufferHandle);
        return nullptr;
    }
    return colorBufferPtr->getBorrowedImageInfo();
}

gfxstream::EmulationGl& FrameBuffer::getEmulationGl() {
    if (!m_emulationGl) {
        GFXSTREAM_ABORT(FatalError(ABORT_REASON_OTHER))
            << "GL/EGL emulation not enabled.";
    }
    return *m_emulationGl;
}

EGLDisplay FrameBuffer::getDisplay() const {
    if (!m_emulationGl) {
        GFXSTREAM_ABORT(FatalError(ABORT_REASON_OTHER))
            << "EGL emulation not enabled.";
    }
    return m_emulationGl->mEglDisplay;
}

EGLSurface FrameBuffer::getWindowSurface() const {
    if (!m_emulationGl) {
        GFXSTREAM_ABORT(FatalError(ABORT_REASON_OTHER))
            << "EGL emulation not enabled.";
    }

    if (!m_emulationGl->mWindowSurface) {
        return EGL_NO_SURFACE;
    }

    const auto* displaySurfaceGl =
        reinterpret_cast<const DisplaySurfaceGl*>(
            m_emulationGl->mWindowSurface->getImpl());

    return displaySurfaceGl->getSurface();
}

EGLContext FrameBuffer::getContext() const {
    if (!m_emulationGl) {
        GFXSTREAM_ABORT(FatalError(ABORT_REASON_OTHER))
            << "EGL emulation not enabled.";
    }
    return m_emulationGl->mEglContext;
}

EGLContext FrameBuffer::getConfig() const {
    if (!m_emulationGl) {
        GFXSTREAM_ABORT(FatalError(ABORT_REASON_OTHER))
            << "EGL emulation not enabled.";
    }
    return m_emulationGl->mEglConfig;
}

EGLContext FrameBuffer::getGlobalEGLContext() const {
    if (!m_emulationGl) {
        GFXSTREAM_ABORT(FatalError(ABORT_REASON_OTHER))
            << "EGL emulation not enabled.";
    }

    if (!m_emulationGl->mPbufferSurface) {
        GFXSTREAM_ABORT(FatalError(ABORT_REASON_OTHER))
            << "FrameBuffer pbuffer surface not available.";
    }

    const auto* displaySurfaceGl =
        reinterpret_cast<const DisplaySurfaceGl*>(
            m_emulationGl->mPbufferSurface->getImpl());

    return displaySurfaceGl->getContextForShareContext();
}

ContextHelper* FrameBuffer::getPbufferSurfaceContextHelper() const {
    if (!m_emulationGl) {
        GFXSTREAM_ABORT(FatalError(ABORT_REASON_OTHER))
            << "EGL emulation not enabled.";
    }
    if (!m_emulationGl->mPbufferSurface) {
        GFXSTREAM_ABORT(FatalError(ABORT_REASON_OTHER))
            << "EGL emulation pbuffer surface not available.";
    }
    const auto* displaySurfaceGl =
        reinterpret_cast<const DisplaySurfaceGl*>(
            m_emulationGl->mPbufferSurface->getImpl());

    return displaySurfaceGl->getContextHelper();
}

void FrameBuffer::logVulkanOutOfMemory(VkResult result, const char* function, int line,
                                       std::optional<uint64_t> allocationSize) {
    m_logger->logMetricEvent(MetricEventVulkanOutOfMemory{
        .vkResultCode = result,
        .function = function,
        .line = std::make_optional(line),
        .allocationSize = allocationSize,
    });
}

const EmulatedEglConfigList* FrameBuffer::getConfigs() const {
    if (!m_emulationGl) {
        GFXSTREAM_ABORT(FatalError(ABORT_REASON_OTHER))
            << "EGL emulation not enabled.";
    }

    return &m_emulationGl->getEmulationEglConfigs();
}

TextureDraw* FrameBuffer::getTextureDraw() const {
    if (!m_emulationGl) {
        GFXSTREAM_ABORT(FatalError(ABORT_REASON_OTHER))
            << "EGL emulation not enabled.";
    }

    return m_emulationGl->mTextureDraw.get();
}

bool FrameBuffer::isFastBlitSupported() const {
    if (!m_emulationGl) {
        GFXSTREAM_ABORT(FatalError(ABORT_REASON_OTHER))
            << "EGL emulation not enabled.";
    }

    return m_emulationGl->isFastBlitSupported();
}

void FrameBuffer::disableFastBlitForTesting() {
    if (!m_emulationGl) {
        GFXSTREAM_ABORT(FatalError(ABORT_REASON_OTHER))
            << "EGL emulation not enabled.";
    }

    m_emulationGl->disableFastBlitForTesting();
}

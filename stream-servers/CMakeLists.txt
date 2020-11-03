# Codec common sources
add_subdirectory(apigen-codec-common)
add_subdirectory(compressedTextureFormats)

# Vulkan
add_subdirectory(vulkan)

# GLES translator
add_subdirectory(glestranslator)
add_subdirectory(libGLSnapshot)

# GLES decoder
add_subdirectory(gles1_dec)
add_subdirectory(gles2_dec)

# GLES dispatch based on Translator
add_subdirectory(OpenGLESDispatch)

# RenderControl decoder
add_subdirectory(renderControl_dec)

# Stream server core
set(stream-server-core-sources
    ChannelStream.cpp
    ColorBuffer.cpp
    FbConfig.cpp
    FenceSync.cpp
    GLESVersionDetector.cpp
    PostWorker.cpp
    ReadbackWorker.cpp
    ReadBuffer.cpp
    render_api.cpp
    RenderChannelImpl.cpp
    RenderThreadInfo.cpp
    RingStream.cpp
    SyncThread.cpp
    TextureDraw.cpp
    TextureResize.cpp
    WindowSurface.cpp
    YUVConverter.cpp
    RenderThread.cpp
    RenderContext.cpp
    RenderControl.cpp
    RenderWindow.cpp
    RenderLibImpl.cpp
    RendererImpl.cpp
    FrameBuffer.cpp)
if (APPLE)
    set(stream-server-core-platform-sources NativeSubWindow_cocoa.m)
elseif (WIN32)
    set(stream-server-core-platform-sources NativeSubWindow_win32.cpp)
else()
    set(stream-server-core-platform-sources NativeSubWindow_x11.cpp)
endif()

add_library(
    gfxstream_backend
    SHARED
    ${stream-server-core-sources}
    ${stream-server-core-platform-sources}
    GfxStreamAgents.cpp
    GfxStreamBackend.cpp
    virtio-gpu-gfxstream-renderer.cpp)
target_link_libraries(
    gfxstream_backend
    PUBLIC
    gfxstream-host-common
    gfxstream-base
    OpenGLESDispatch
    gles1_dec
    gles2_dec
    renderControl_dec
    gfxstream-vulkan-server
    gfxstream-snapshot
    apigen-codec-common)
target_include_directories(
    gfxstream_backend
    PUBLIC
    ${GFXSTREAM_REPO_ROOT}
    ${GFXSTREAM_REPO_ROOT}/include
    ${GFXSTREAM_REPO_ROOT}/stream-servers
    ${GFXSTREAM_REPO_ROOT}/stream-servers/apigen-codec-common
    ${GFXSTREAM_REPO_ROOT}/stream-servers/vulkan)
android_install_shared(gfxstream_backend)

# Testing libraries
add_subdirectory(testlibs)

# Backend unit tests
add_executable(
    gfxstream_backend_unittests
    gfxstream_unittest.cpp)
target_link_libraries(
    gfxstream_backend_unittests
    PRIVATE
    OSWindow
    gfxstream_backend
    gfxstream-base
    gtest_main)

# Framebuffer unit tests
add_library(
    stream-server-testing-support
    ${stream-server-core-sources}
    ${stream-server-core-platform-sources}
    tests/SampleApplication.cpp)
target_include_directories(
    stream-server-testing-support
    PRIVATE
    ${GFXSTREAM_REPO_ROOT}
    PUBLIC
    ${GFXSTREAM_REPO_ROOT}/base/testing
    ${GFXSTREAM_REPO_ROOT}
    ${GFXSTREAM_REPO_ROOT}/include
    ${GFXSTREAM_REPO_ROOT}/stream-servers
    ${GFXSTREAM_REPO_ROOT}/stream-servers/apigen-codec-common
    ${GFXSTREAM_REPO_ROOT}/stream-servers/vulkan)
target_link_libraries(
    stream-server-testing-support
    PUBLIC
    gfxstream-base
    gfxstream-host-common
    OpenGLESDispatch
    gles1_dec
    gles2_dec
    renderControl_dec
    gfxstream-vulkan-server
    gfxstream-snapshot
    apigen-codec-common
    OSWindow)
add_executable(
    OpenglRender_unittests
    tests/FrameBuffer_unittest.cpp
    tests/GLSnapshotTesting.cpp
    tests/OpenGLTestContext.cpp
    tests/GLTestUtils.cpp
    tests/ShaderUtils.cpp)
target_link_libraries(
    OpenglRender_unittests
    PRIVATE
    gfxstream-base-testing-support
    stream-server-testing-support
    gfxstream-host-common-testing-support
    gtest)

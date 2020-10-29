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
set(gfxstream-server-common-sources
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
    RenderWindow.cpp
    RenderLibImpl.cpp
    RendererImpl.cpp
    FrameBuffer.cpp)
if (APPLE)
    set(gfxstream-server-platform-sources NativeSubWindow_cocoa.m)
elseif (WIN32)
    set(gfxstream-server-platform-sources NativeSubWindow_win32.cpp)
else()
    set(gfxstream-server-platform-sources NativeSubWindow_x11.cpp)
endif()

add_library(
    gfxstream-server
    ${gfxstream-server-common-sources}
    ${gfxstream-server-platform-sources})
target_link_libraries(
    gfxstream-server
    gfxstream-host-common
    gfxstream-base
    OpenGLESDispatch
    glestranslator
    gles1_dec
    gles2_dec
    renderControl_dec
    gfxstream-vulkan-server
    apigen-codec-common)
target_include_directories(
    gfxstream-server
    PRIVATE
    ${GFXSTREAM_REPO_ROOT}
    ${GFXSTREAM_REPO_ROOT}/include
    ${GFXSTREAM_REPO_ROOT}/stream-servers
    ${GFXSTREAM_REPO_ROOT}/stream-servers/apigen-codec-common
    ${GFXSTREAM_REPO_ROOT}/stream-servers/vulkan)


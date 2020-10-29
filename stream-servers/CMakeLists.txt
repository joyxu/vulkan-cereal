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
add_library(
    gfxstream-server
    ChannelStream.cpp)
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


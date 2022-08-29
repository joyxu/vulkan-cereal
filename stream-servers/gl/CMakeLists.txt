# GLES decoder
add_subdirectory(gles1_dec)
add_subdirectory(gles2_dec)

# GLES dispatch based on Translator
add_subdirectory(OpenGLESDispatch)

# GLES translator
add_subdirectory(glestranslator)

add_library(gfxstream-gl-server
            CompositorGl.cpp
            EmulatedEglConfig.cpp
            TextureDraw.cpp
            TextureResize.cpp)

target_link_libraries(
        gfxstream-gl-server
        PUBLIC
        gfxstream-base.headers
        gfxstream_vulkan_headers
        gles1_dec
        gles2_dec
        OpenGLESDispatch)

target_include_directories(gfxstream-gl-server
                           PRIVATE
                           ${GFXSTREAM_REPO_ROOT}
                           ${GFXSTREAM_REPO_ROOT}/gldispatch/include
                           ${GFXSTREAM_REPO_ROOT}/include
                           ${GFXSTREAM_REPO_ROOT}/stream-servers
                           ${GFXSTREAM_REPO_ROOT}/stream-servers/gl)
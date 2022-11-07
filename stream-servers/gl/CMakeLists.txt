# GLES decoder
add_subdirectory(gles1_dec)
add_subdirectory(gles2_dec)

# GLES dispatch based on Translator
add_subdirectory(OpenGLESDispatch)

# GLES translator
add_subdirectory(glestranslator)
add_subdirectory(glsnapshot)

add_library(gfxstream-gl-server
            ColorBufferGl.cpp
            CompositorGl.cpp
            DisplayGl.cpp
            DisplaySurfaceGl.cpp
            EmulatedEglConfig.cpp
            EmulatedEglContext.cpp
            EmulatedEglImage.cpp
            EmulatedEglWindowSurface.cpp
            EmulationGl.cpp
            GLESVersionDetector.cpp
            TextureDraw.cpp
            TextureResize.cpp
            YUVConverter.cpp)

target_link_libraries(
        gfxstream-gl-server
        PUBLIC
        aemu-base.headers
        gfxstream_vulkan_headers
        gfxstream_egl_headers
        gles1_dec
        gles2_dec
        GLSnapshot
        OpenGLESDispatch)

target_include_directories(gfxstream-gl-server
                           PRIVATE
                           ${GFXSTREAM_REPO_ROOT}
                           ${GFXSTREAM_REPO_ROOT}/gldispatch/include
                           ${GFXSTREAM_REPO_ROOT}/include
                           ${GFXSTREAM_REPO_ROOT}/stream-servers
                           ${GFXSTREAM_REPO_ROOT}/stream-servers/gl
                           ${GFXSTREAM_REPO_ROOT}/third-party/glm/include)

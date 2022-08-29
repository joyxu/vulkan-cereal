# GLES decoder
add_subdirectory(gles1_dec)
add_subdirectory(gles2_dec)

add_library(gfxstream-gl-server
            EmulatedEglConfig.cpp)

target_link_libraries(
        gfxstream-gl-server
        PUBLIC
        gfxstream-base.headers
        gles1_dec
        gles2_dec)

target_include_directories(gfxstream-gl-server
                           PRIVATE
                           ${GFXSTREAM_REPO_ROOT}
                           ${GFXSTREAM_REPO_ROOT}/gldispatch/include
                           ${GFXSTREAM_REPO_ROOT}/include
                           ${GFXSTREAM_REPO_ROOT}/stream-servers
                           ${GFXSTREAM_REPO_ROOT}/stream-servers/gl)
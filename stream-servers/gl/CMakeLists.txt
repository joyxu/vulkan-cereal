add_library(gfxstream-gl-server
            EmulatedEglConfig.cpp)

target_link_libraries(
        gfxstream-gl-server
        PUBLIC
        gfxstream-base.headers)

target_include_directories(gfxstream-gl-server
                           PRIVATE
                           ${GFXSTREAM_REPO_ROOT}
                           ${GFXSTREAM_REPO_ROOT}/gldispatch/include
                           ${GFXSTREAM_REPO_ROOT}/include
                           ${GFXSTREAM_REPO_ROOT}/stream-servers
                           ${GFXSTREAM_REPO_ROOT}/stream-servers/gl)
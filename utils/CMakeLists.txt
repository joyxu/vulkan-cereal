add_library(gfxstream_utils.headers INTERFACE)

target_include_directories(gfxstream_utils.headers INTERFACE include)
target_link_libraries(
    gfxstream_utils.headers
    INTERFACE
    renderdoc
    gfxstream_vulkan_headers
    aemu-host-common.headers
    aemu-base.headers)

if (ENABLE_VKCEREAL_TESTS)
    add_executable(
        gfxstream_utils_unittests
        RenderDoc_unittest.cpp)

    target_link_libraries(
        gfxstream_utils_unittests
        PRIVATE
        gfxstream_utils.headers
        ${GFXSTREAM_HOST_COMMON_LIB}
        ${GFXSTREAM_BASE_LIB}
        renderdoc
        gtest_main
        gmock_main)

    gtest_discover_tests(gfxstream_utils_unittests)
endif()

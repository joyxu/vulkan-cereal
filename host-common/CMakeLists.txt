if (APPLE)
    set(gfxstream-host-common-opengles-platform-sources
        opengl/macTouchOpengl.m
        opengl/NativeGpuInfo_darwin.cpp)
elseif (WIN32)
    set(gfxstream-host-common-opengles-platform-sources
        opengl/NativeGpuInfo_windows.cpp)
else()
    set(gfxstream-host-common-opengles-platform-sources
        opengl/NativeGpuInfo_linux.cpp)
endif()

# TODO(gregschlom) move this to base
add_library(logging-base
    STATIC
    logging.cpp
    GfxstreamFatalError.cpp
    ../base/Metrics.cpp)
target_include_directories(
    logging-base
    PRIVATE
    include/host-common
    ${GFXSTREAM_REPO_ROOT}
    ${GFXSTREAM_REPO_ROOT}/include)
target_link_libraries(
    logging-base
    PUBLIC
    gfxstream_vulkan_headers
    PRIVATE
    gfxstream-base.headers
    gfxstream-host-common.headers)

add_library(gfxstream-host-common.headers INTERFACE)
target_link_libraries(
    gfxstream-host-common.headers
    INTERFACE
    gfxstream-snapshot.headers
    renderdoc)
target_include_directories(
    gfxstream-host-common.headers
    INTERFACE
    include
    ${GFXSTREAM_REPO_ROOT}/include)

if (BUILD_STANDALONE)
    add_library(
        gfxstream-host-common
        STATIC

        # emugl glue
        crash_reporter.cpp
        vm_operations.cpp
        feature_control.cpp
        dma_device.cpp
        sync_device.cpp
        misc.cpp
        window_operations.cpp

        # What used to be android-emu
        AndroidPipe.cpp
        HostmemIdMapping.cpp
        RefcountPipe.cpp
        GraphicsAgentFactory.cpp

        # goldfish sync
        GoldfishSyncCommandQueue.cpp
        goldfish_sync.cpp

        # goldfish dma
        DmaMap.cpp
        GoldfishDma.cpp

        # Address space device
        address_space_device_control_ops.cpp
        address_space_device.cpp
        address_space_host_memory_allocator.cpp
        address_space_shared_slots_host_memory_allocator.cpp
        address_space_graphics.cpp
        address_space_host_media.cpp

        hw-config.cpp

        # general opengles host stuff, incl process pipe
        # and opengl es pipe
        opengles.cpp
        opengl/EmuglBackendList.cpp
        # opengl/EmuglBackendList_unittest.cpp
        opengl/emugl_config.cpp
        # opengl/emugl_config_unittest.cpp
        opengl/GLProcessPipe.cpp
        opengl/GpuFrameBridge.cpp
        # opengl/GpuFrameBridge_unittest.cpp
        opengl/gpuinfo.cpp
        # opengl/gpuinfo_unittest.cpp
        opengl/logger.cpp
        opengl/OpenglEsPipe.cpp
        ${gfxstream-host-common-opengles-platform-sources}
        )

    target_include_directories(
        gfxstream-host-common
        PRIVATE
        include/host-common
        include/host-common/opengl
        ${GFXSTREAM_REPO_ROOT}
        ${GFXSTREAM_REPO_ROOT}/include)
    target_link_libraries(
        gfxstream-host-common
        PUBLIC
        gfxstream-base.headers
        gfxstream-host-common.headers
        ${GFXSTREAM_BASE_LIB}
        PRIVATE
        logging-base
    )
endif()

if(NOT TARGET gfxstream-host-common.product-feature-override)
    add_library(gfxstream-host-common.product-feature-override OBJECT FeatureControlOverride.cpp)
    target_link_libraries(
        gfxstream-host-common.product-feature-override
        PRIVATE
        gfxstream-host-common.headers
        gfxstream-base.headers)
endif()

target_link_libraries(
    ${GFXSTREAM_HOST_COMMON_LIB}
    PRIVATE
    gfxstream-host-common.product-feature-override)

# Tests
add_library(
    gfxstream-host-common-testing-support
    testing/HostAddressSpace.cpp
    testing/MockGraphicsAgentFactory.cpp
    testing/MockAndroidEmulatorWindowAgent.cpp
    testing/MockAndroidMultiDisplayAgent.cpp
    testing/MockAndroidVmOperations.cpp)
target_include_directories(
    gfxstream-host-common-testing-support
    PRIVATE
    ${GFXSTREAM_REPO_ROOT})
target_link_libraries(
    gfxstream-host-common-testing-support
    PUBLIC
    gfxstream_vulkan_headers
    PRIVATE
    gfxstream-base.headers
    gfxstream-host-common.headers
    gtest
    gmock)

if (ENABLE_VKCEREAL_TESTS)
    add_executable(
        gfxstream-host-common_unittests
        address_space_graphics_unittests.cpp
        address_space_host_memory_allocator_unittests.cpp
        address_space_shared_slots_host_memory_allocator_unittests.cpp
        HostAddressSpace_unittest.cpp
        HostmemIdMapping_unittest.cpp
        logging_unittest.cpp
        GfxstreamFatalError_unittest.cpp
        RenderDoc_unittest.cpp)

    target_include_directories(
        gfxstream-host-common_unittests
        PRIVATE
        ${GFXSTREAM_REPO_ROOT}
        ${GFXSTREAM_REPO_ROOT}/include)

    target_link_libraries(
        gfxstream-host-common_unittests
        PRIVATE
        gfxstream-base.headers
        gfxstream-host-common.headers
        ${GFXSTREAM_BASE_LIB}
        ${GFXSTREAM_HOST_COMMON_LIB}
        gfxstream-host-common-testing-support
        gtest_main
        gmock_main)

    gtest_discover_tests(gfxstream-host-common_unittests)
endif()

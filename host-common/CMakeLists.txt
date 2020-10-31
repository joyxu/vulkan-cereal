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

add_library(
    gfxstream-host-common
    STATIC

    # emugl glue
    address_space_device_control_ops.cpp
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

    # android-emu avd globals
    avd/info.cpp

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
    ${GFXSTREAM_REPO_ROOT}
    ${GFXSTREAM_REPO_ROOT}/include
    ${GFXSTREAM_REPO_ROOT}/stream-servers)

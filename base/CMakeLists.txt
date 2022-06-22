if (WIN32)
endif()

add_library(gfxstream-base.headers INTERFACE)
# TODO: Remove this once host-common/logging.h is moved into base.
target_link_libraries(gfxstream-base.headers INTERFACE gfxstream-host-common.headers)
target_include_directories(gfxstream-base.headers INTERFACE include)

if (BUILD_STANDALONE)
    set(gfxstream-base-common-sources
        AlignedBuf.cpp
        CompressingStream.cpp
        CpuTime.cpp
        DecompressingStream.cpp
        FileUtils.cpp
        FunctorThread.cpp
        GLObjectCounter.cpp
        LayoutResolver.cpp
        MemStream.cpp
        StdioStream.cpp
        MemoryTracker.cpp
        MessageChannel.cpp
        PathUtils.cpp
        ring_buffer.cpp
        SharedLibrary.cpp
        StringFormat.cpp
        Stream.cpp
        StreamSerializing.cpp
        SubAllocator.cpp
        System.cpp
        Tracing.cpp)
    set(gfxstream-base-posix-sources
        SharedMemory_posix.cpp
        Thread_pthread.cpp)
    set(gfxstream-base-windows-sources
        msvc.cpp
        SharedMemory_win32.cpp
        Thread_win32.cpp
        Win32UnicodeString.cpp)

    if (APPLE)
        set(gfxstream-platform-sources
            ${gfxstream-base-posix-sources}
            system-native-mac.mm)
    elseif (WIN32)
        set(gfxstream-platform-sources
            ${gfxstream-base-windows-sources})
    else()
        set(gfxstream-platform-sources
            ${gfxstream-base-posix-sources})
    endif()

    add_library(
        gfxstream-base
        ${gfxstream-base-common-sources}
        ${gfxstream-platform-sources})

    if (WIN32)
        set(gfxstream-base-platform-deps "")
    else()
        set(gfxstream-base-platform-deps dl)
    endif()

    target_link_libraries(
        gfxstream-base
        PUBLIC
        gfxstream-base.headers
        # TODO(joshuaduong): Remove this when logging.h is moved into base
        gfxstream-host-common.headers
        PRIVATE
        lz4
        perfetto-tracing-only
        ${gfxstream-base-platform-deps})

    target_include_directories(
        gfxstream-base
        PRIVATE
        ${GFXSTREAM_REPO_ROOT}/include
        PUBLIC
        ${GFXSTREAM_REPO_ROOT})
    if (NOT MSVC)
        target_compile_options(
            gfxstream-base PRIVATE -fvisibility=default)
    endif()
endif()

if (APPLE)
    set(gfxstream-base-platform-test-sources "")
elseif (WIN32)
    set(gfxstream-base-platform-test-sources Win32UnicodeString_unittest.cpp)
else()
    set(gfxstream-base-platform-test-sources "")
endif()

# Tests
add_library(
    gfxstream-base-testing-support
    testing/file_io.cpp)
target_include_directories(
    gfxstream-base-testing-support
    PRIVATE
    ${GFXSTREAM_REPO_ROOT}
    ${GFXSTREAM_REPO_ROOT}/include
    PUBLIC
    ${GFXSTREAM_REPO_ROOT}/base/testing)
target_link_libraries(
    gfxstream-base-testing-support
    PRIVATE
    gfxstream-base.headers
    gtest
    gmock)

if (ENABLE_VKCEREAL_TESTS)
    add_executable(
        gfxstream-base_unittests
        AlignedBuf_unittest.cpp
        HealthMonitor_unittest.cpp
        ArraySize_unittest.cpp
        LayoutResolver_unittest.cpp
        LruCache_unittest.cpp
        Optional_unittest.cpp
        ring_buffer_unittest.cpp
        StringFormat_unittest.cpp
        SubAllocator_unittest.cpp
        TypeTraits_unittest.cpp)
    target_link_libraries(
        gfxstream-base_unittests
        PRIVATE
        gfxstream-base.headers
        ${GFXSTREAM_BASE_LIB}
        logging-base
        gfxstream-base-testing-support
        gmock
        gtest_main)
    gtest_discover_tests(gfxstream-base_unittests)
endif()

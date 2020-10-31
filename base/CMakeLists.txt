if (WIN32)
    add_subdirectory(msvc-posix-compat)
endif()

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

target_link_libraries(
    gfxstream-base PRIVATE lz4)
target_include_directories(
    gfxstream-base PUBLIC ${GFXSTREAM_REPO_ROOT})
target_compile_options(
    gfxstream-base PRIVATE -fvisibility=default)

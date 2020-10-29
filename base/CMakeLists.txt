if (WIN32)
    add_subdirectory(msvc-posix-compat)
endif()

set(gfxstream-base-common-sources
    CompressingStream.cpp
    CpuTime.cpp
    DecompressingStream.cpp
    FunctorThread.cpp
    GLObjectCounter.cpp
    LayoutResolver.cpp
    MemStream.cpp
    MemoryTracker.cpp
    MessageChannel.cpp
    PathUtils.cpp
    ring_buffer.c
    SharedLibrary.cpp
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
if (WIN32)
add_library(
    gfxstream-base
    ${gfxstream-base-common-sources}
    ${gfxstream-base-windows-sources})
else()
add_library(
    gfxstream-base
    ${gfxstream-base-common-sources}
    ${gfxstream-base-posix-sources})
endif()

target_link_libraries(
    gfxstream-base PRIVATE lz4)
target_include_directories(
    gfxstream-base PUBLIC ${GFXSTREAM_REPO_ROOT})

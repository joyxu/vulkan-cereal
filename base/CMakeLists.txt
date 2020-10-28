if (WIN32)
    add_subdirectory(msvc-posix-compat)
endif()

set(gfxstream-base-common-sources
    CompressingStream.cpp
    DecompressingStream.cpp
    GLObjectCounter.cpp
    MemStream.cpp
    MemoryTracker.cpp
    MessageChannel.cpp
    PathUtils.cpp
    SharedLibrary.cpp
    Stream.cpp
    StreamSerializing.cpp
    SubAllocator.cpp
    System.cpp
    Tracing.cpp)
set(gfxstream-base-posix-sources
    Thread_pthread.cpp)
set(gfxstream-base-windows-sources
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

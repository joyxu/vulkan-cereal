set(gfxstream-base-common-sources
    MemStream.cpp
    MessageChannel.cpp
    PathUtils.cpp
    SharedLibrary.cpp
    Stream.cpp
    StreamSerializing.cpp
    SubAllocator.cpp
    System.cpp
    Tracing.cpp)
set(gfxstream-base-windows-sources
    Win32UnicodeString.cpp)

if (WIN32)
add_library(
    gfxstream-base
    ${gfxstream-base-common-sources}
    ${gfxstream-base-windows-sources})
else()
add_library(
    gfxstream-base
    ${gfxstream-base-common-sources})
endif()

target_include_directories(
    gfxstream-base PUBLIC ${GFXSTREAM_REPO_ROOT})

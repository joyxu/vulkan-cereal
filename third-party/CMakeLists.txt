add_subdirectory(angle)

# TODO(gregschlom) Remove this in favor of astc-encoder
add_subdirectory(astc-codec)

# Configure and add `astc-encoder`, if needed
if (ASTC_CPU_DECODING)
    set(DECOMPRESSOR ON)   # Disable compression code
    set(CLI OFF)           # Disable the command line interface

    # Compile with the AVX2 instruction set. This is the fastest option available on x86_64.
    # At run time, if the CPU doesn't support AVX2, the library will simply return an error status
    # during initialization and we will fall back on the compute shader to decompress ASTC textures.
    #
    # In the future, we should define `ASTCENC_DYNAMIC_LIBRARY` and build multiple versions of the
    # library for each SIMD architecture, and dynamically load the fastest one at run time.
    # See also: https://github.com/ARM-software/astc-encoder/issues/79
    set(ISA_AVX2 ON)

    add_subdirectory(astc-encoder)
endif ()

add_subdirectory(lz4)
add_subdirectory(perfetto-tracing-only)
add_subdirectory(renderdoc)
add_subdirectory(stb)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
add_subdirectory(googletest)

set(PROJECT_TO_FETCH)

if(NOT TARGET gfxstream_vulkan_headers)
    if(DEPENDENCY_RESOLUTION STREQUAL "AOSP")
        set(GFXSTREAM_PROTOCOLS_PATH ${PROJECT_SOURCE_DIR}/../../../external/gfxstream-protocols)
        if(EXISTS ${GFXSTREAM_PROTOCOLS_PATH})
            add_subdirectory(${GFXSTREAM_PROTOCOLS_PATH} gfxstream-protocols)
        endif()
    elseif(DEPENDENCY_RESOLUTION STREQUAL "SYSTEM")
        message(FATAL_ERROR "Not yet implemented")
    elseif(DEPENDENCY_RESOLUTION STREQUAL "DOWNLOAD")
        FetchContent_Declare(
            gfxstream_protocols
            GIT_REPOSITORY https://android.googlesource.com/platform/external/gfxstream-protocols
            GIT_REMOTE_NAME aosp
            GIT_TAG aosp/master
            GIT_REMOTE_UPDATE_STRATEGY CHECKOUT
            GIT_PROGRESS TRUE
        )
        list(APPEND PROJECT_TO_FETCH gfxstream_protocols)
    endif()
endif()

if(NOT TARGET flatbuffers)
    option(FLATBUFFERS_BUILD_TESTS "" OFF)
    option(FLATBUFFERS_BUILD_CPP17 "" ON)

    if(DEPENDENCY_RESOLUTION STREQUAL "AOSP")
        set(FLATBUFFERS_PATH ${PROJECT_SOURCE_DIR}/../../../external/flatbuffers)
        if(EXISTS ${FLATBUFFERS_PATH})
            add_subdirectory(${FLATBUFFERS_PATH} flatbuffers)
        endif()
    elseif(DEPENDENCY_RESOLUTION STREQUAL "SYSTEM")
        find_package(FlatBuffers 2.0.8 REQUIRED CONFIG)
        # Promote the targets to global.
        set_property(TARGET flatbuffers::flatbuffers PROPERTY IMPORTED_GLOBAL TRUE)
        set_property(TARGET flatbuffers::flatc PROPERTY IMPORTED_GLOBAL TRUE)
        add_library(flatbuffers ALIAS flatbuffers::flatbuffers)
        add_executable(flatc ALIAS flatbuffers::flatc)
    elseif(DEPENDENCY_RESOLUTION STREQUAL "DOWNLOAD")
        FetchContent_Declare(
            flatbuffers
            GIT_REPOSITORY https://android.googlesource.com/platform/external/flatbuffers
            GIT_REMOTE_NAME aosp
            GIT_TAG aosp/master
            GIT_REMOTE_UPDATE_STRATEGY CHECKOUT
            GIT_PROGRESS TRUE
        )
        list(APPEND PROJECT_TO_FETCH flatbuffers)
    endif()
endif()

if(PROJECT_TO_FETCH)
    FetchContent_MakeAvailable(${PROJECT_TO_FETCH})
endif()

if(NOT TARGET gfxstream_vulkan_headers)
    message(FATAL_ERROR "The dependency gfxstream_vulkan_headers not found")
endif()
if(NOT TARGET flatbuffers)
    message(FATAL_ERROR "The dependency flatbuffers not found.")
endif()

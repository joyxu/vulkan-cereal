add_subdirectory(angle)

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

if(NOT TARGET aemu_common)
    set(AEMU_COMMON_BUILD_CONFIG "gfxstream" CACHE STRING "")
    if(DEPENDENCY_RESOLUTION STREQUAL "AOSP")
        set(AEMU_COMMON_PATH ${PROJECT_SOURCE_DIR}/../../../hardware/google/aemu)
        if(EXISTS ${AEMU_COMMON_PATH})
            add_subdirectory(${AEMU_COMMON_PATH} aemu_common)
        endif()
    elseif(DEPENDENCY_RESOLUTION STREQUAL "SYSTEM")
        message(FATAL_ERROR "aemu_common not yet implemented for DEPENDENCY_RESOLUTION=SYSTEM")
    elseif(DEPENDENCY_RESOLUTION STREQUAL "DOWNLOAD")
        FetchContent_Declare(
            aemu_common
            GIT_REPOSITORY https://android.googlesource.com/platform/hardware/google/aemu
            GIT_REMOTE_NAME aosp
            GIT_TAG aosp/master
            GIT_REMOTE_UPDATE_STRATEGY CHECKOUT
            GIT_PROGRESS TRUE
        )
        list(APPEND PROJECT_TO_FETCH aemu_common)
    endif()
endif()

if(NOT TARGET gfxstream_vulkan_headers)
    if(DEPENDENCY_RESOLUTION STREQUAL "AOSP")
        set(GFXSTREAM_PROTOCOLS_PATH ${PROJECT_SOURCE_DIR}/../../../external/gfxstream-protocols)
        if(EXISTS ${GFXSTREAM_PROTOCOLS_PATH})
            add_subdirectory(${GFXSTREAM_PROTOCOLS_PATH} gfxstream-protocols)
        endif()
    elseif(DEPENDENCY_RESOLUTION STREQUAL "SYSTEM")
        find_package(Vulkan 1.2.198 REQUIRED)
        add_library(gfxstream_vulkan_headers INTERFACE)
        target_include_directories(gfxstream_vulkan_headers INTERFACE ${Vulkan_INCLUDE_DIRS})
        target_compile_definitions(gfxstream_vulkan_headers
                                   INTERFACE VK_GFXSTREAM_STRUCTURE_TYPE_EXT)
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
        find_package(PkgConfig REQUIRED)
        pkg_search_module(flatbuffers REQUIRED IMPORTED_TARGET GLOBAL flatbuffers>=2.0.6)
        add_library(flatbuffers ALIAS PkgConfig::flatbuffers)
        find_program(FLATC flatc NO_CACHE REQUIRED)
        add_executable(flatc IMPORTED GLOBAL)
        set_property(TARGET flatc PROPERTY IMPORTED_LOCATION ${FLATC})
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

if(NOT TARGET aemu_common)
    message(FATAL_ERROR "The dependency aemu_common not found")
endif()
if(NOT TARGET gfxstream_vulkan_headers)
    message(FATAL_ERROR "The dependency gfxstream_vulkan_headers not found")
endif()
if(NOT TARGET flatbuffers)
    message(FATAL_ERROR "The dependency flatbuffers not found.")
endif()

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

set(GFXSTREAM_PROTOCOLS_PATH ${PROJECT_SOURCE_DIR}/../../../external/gfxstream-protocols)
if (EXISTS ${GFXSTREAM_PROTOCOLS_PATH})
    add_subdirectory(${GFXSTREAM_PROTOCOLS_PATH} gfxstream-protocols)
else()
    FetchContent_Declare(
        gfxstream_protocols
        GIT_REPOSITORY https://android.googlesource.com/platform/external/gfxstream-protocols
        GIT_REMOTE_NAME aosp
        GIT_TAG aosp/master
        GIT_REMOTE_UPDATE_STRATEGY CHECKOUT
    )
    FetchContent_MakeAvailable(gfxstream_protocols)
endif()

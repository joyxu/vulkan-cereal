if (ASTC_CPU_DECODING)
    set(astc-cpu-decompressor-sources AstcCpuDecompressorImpl.cpp)
else()
    set(astc-cpu-decompressor-sources AstcCpuDecompressorNoOp.cpp)
endif()

add_library(
    gfxstream-compressedTextures
    ${astc-cpu-decompressor-sources}
    etc.cpp)
target_include_directories(
    gfxstream-compressedTextures PUBLIC ../
    ${GFXSTREAM_REPO_ROOT}/vulkan
    ${GFXSTREAM_REPO_ROOT}/vulkan/cereal
    PRIVATE
    ${GFXSTREAM_REPO_ROOT}
    ${GFXSTREAM_REPO_ROOT}/include
    ${GFXSTREAM_REPO_ROOT}/stream-servers
    ${GFXSTREAM_REPO_ROOT}/stream-servers/vulkan
)


target_link_libraries(gfxstream-compressedTextures PRIVATE
    gfxstream-base.headers
    gfxstream_vulkan_headers)

if(ASTC_CPU_DECODING)
    target_link_libraries(gfxstream-compressedTextures PRIVATE astcdec-avx2-static)
endif()

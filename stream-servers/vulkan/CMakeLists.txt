add_subdirectory(cereal)

if (ASTC_CPU_DECODING)
    set(astc-cpu-decompressor-sources AstcCpuDecompressorImpl.cpp)
else()
    set(astc-cpu-decompressor-sources  AstcCpuDecompressorNoOp.cpp)
endif()

add_library(gfxstream-vulkan-server
            ${astc-cpu-decompressor-sources}
            BorrowedImageVk.cpp
            CompositorVk.cpp
            DisplayVk.cpp
            DebugUtilsHelper.cpp
            SwapChainStateVk.cpp
            VkAndroidNativeBuffer.cpp
            VkCommonOperations.cpp
            VkDecoder.cpp
            VkDecoderGlobalState.cpp
            VkDecoderSnapshot.cpp
            VkFormatUtils.cpp
            VkReconstruction.cpp
            VulkanDispatch.cpp
            VulkanHandleMapping.cpp
            VulkanStream.cpp
            vk_util.cpp)
set_source_files_properties(VkDecoder.cpp PROPERTIES COMPILE_FLAGS -Wno-unused-variable)

target_link_libraries(gfxstream-vulkan-server PUBLIC
        OpenglRender_vulkan_cereal
        gfxstream-compressedTextures
        apigen-codec-common
        gfxstream-base.headers
        gfxstream-snapshot.headers
        gfxstream_vulkan_headers)
if(ASTC_CPU_DECODING)
    target_link_libraries(gfxstream-vulkan-server PRIVATE astcdec-avx2-static)
endif()

if (WIN32)
target_compile_definitions(gfxstream-vulkan-server PRIVATE -DVK_USE_PLATFORM_WIN32_KHR)
elseif (APPLE)
target_compile_definitions(gfxstream-vulkan-server PRIVATE -DVK_USE_PLATFORM_METAL_EXT)
endif()

if (NOT MSVC)
    target_compile_options(gfxstream-vulkan-server PRIVATE -fvisibility=hidden)
endif()
target_compile_options(gfxstream-vulkan-server PRIVATE -Wno-unused-value -Wno-return-type -Wno-return-type-c-linkage)

target_include_directories(gfxstream-vulkan-server
                           PUBLIC
                           ${GFXSTREAM_REPO_ROOT}/vulkan
                           ${GFXSTREAM_REPO_ROOT}/vulkan/cereal
                           PRIVATE
                           ${GFXSTREAM_REPO_ROOT}
                           ${GFXSTREAM_REPO_ROOT}/include
                           ${GFXSTREAM_REPO_ROOT}/stream-servers
                           ${GFXSTREAM_REPO_ROOT}/stream-servers/vulkan
                           ${GFXSTREAM_REPO_ROOT}/stream-servers/apigen-codec-common)

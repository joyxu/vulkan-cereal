add_subdirectory(cereal)

add_library(gfxstream-vulkan-server
            VkAndroidNativeBuffer.cpp
            VkCommonOperations.cpp
            VkDecoder.cpp
            VkDecoderGlobalState.cpp
            VkDecoderSnapshot.cpp
            VkReconstruction.cpp
            VulkanDispatch.cpp
            VulkanHandleMapping.cpp
            VulkanStream.cpp)
target_link_libraries(gfxstream-vulkan-server PUBLIC OpenglRender_vulkan_cereal gfxstream-compressedTextures apigen-codec-common gfxstream-base)

if (WIN32)
android_target_compile_definitions(gfxstream-vulkan-server windows PRIVATE -DVK_USE_PLATFORM_WIN32_KHR)
endif()

target_compile_options(gfxstream-vulkan-server PRIVATE -fvisibility=hidden -Wno-unused-value -Wno-return-type -Wno-return-type-c-linkage)

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

add_subdirectory(cereal)

add_library(stream_server_vulkan
            VkAndroidNativeBuffer.cpp
            VkCommonOperations.cpp
            VkDecoder.cpp
            VkDecoderGlobalState.cpp
            VkDecoderSnapshot.cpp
            VkReconstruction.cpp
            VulkanDispatch.cpp
            VulkanHandleMapping.cpp
            VulkanStream.cpp)
target_link_libraries(stream_server_vulkan PUBLIC OpenglRender_vulkan_cereal gfxstream-compressedTextures)

if (WIN32)
android_target_compile_definitions(stream_server_vulkan windows PRIVATE -DVK_USE_PLATFORM_WIN32_KHR)
endif()

target_compile_options(stream_server_vulkan PRIVATE -fvisibility=hidden -Wno-unused-value -Wno-return-type -Wno-return-type-c-linkage)

target_include_directories(stream_server_vulkan
                           PUBLIC . ./cereal
                           PRIVATE . ../ ../../include)

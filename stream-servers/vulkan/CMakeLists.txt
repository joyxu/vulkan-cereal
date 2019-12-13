add_subdirectory(cereal)

# add_library(gfxstreamvulkanserver
#             VkAndroidNativeBuffer.cpp
#             VkCommonOperations.cpp
#             VkDecoder.cpp
#             VkDecoderGlobalState.cpp
#             VkDecoderSnapshot.cpp
#             # VkReconstruction.cpp
#             VulkanDispatch.cpp
#             VulkanHandleMapping.cpp
#             VulkanStream.cpp)
# target_link_libraries(gfxstreamvulkanserver PUBLIC OpenglRender_vulkan_cereal)
# 
# # android_target_compile_definitions(gfxstreamvulkanserver windows PRIVATE -DVK_USE_PLATFORM_WIN32_KHR)
# 
# target_compile_options(gfxstreamvulkanserver PRIVATE -fvisibility=hidden -Wno-unused-value -Wno-return-type -Wno-return-type-c-linkage)
# 
# target_include_directories(gfxstreamvulkanserver
#                            PUBLIC . ./cereal
#                            PRIVATE . ../ ../../include)

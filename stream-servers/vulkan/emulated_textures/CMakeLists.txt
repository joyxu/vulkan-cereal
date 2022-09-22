add_library(emulated_textures
        "AstcTexture.cpp"
        )

target_link_libraries(emulated_textures PUBLIC
        OpenglRender_vulkan_cereal
        gfxstream-base.headers
        gfxstream_vulkan_headers)

target_include_directories(emulated_textures
        PRIVATE
        ${GFXSTREAM_REPO_ROOT}
        ${GFXSTREAM_REPO_ROOT}/include
        ${GFXSTREAM_REPO_ROOT}/stream-servers
        ${GFXSTREAM_REPO_ROOT}/stream-servers/vulkan)

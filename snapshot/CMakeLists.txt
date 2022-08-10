add_library(
    gfxstream-snapshot
    TextureLoader.cpp
    TextureSaver.cpp)
target_link_libraries(
    gfxstream-snapshot
    PRIVATE
    gfxstream-base.headers
    gfxstream-host-common.headers)
target_include_directories(
    gfxstream-snapshot
    PUBLIC
    ${GFXSTREAM_REPO_ROOT})
if (APPLE)
    target_compile_definitions(
        gfxstream-snapshot PRIVATE -Dfseeko64=fseek -Dftello64=ftell)
endif()

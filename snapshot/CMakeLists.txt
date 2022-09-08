add_library(gfxstream-snapshot.headers INTERFACE)
target_include_directories(gfxstream-snapshot.headers INTERFACE include)

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
    ${GFXSTREAM_REPO_ROOT}/include)
if (APPLE)
    target_compile_definitions(
        gfxstream-snapshot PRIVATE -Dfseeko64=fseek -Dftello64=ftell)
endif()

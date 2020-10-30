add_library(
    apigen-codec-common
    ChecksumCalculator.cpp
    ChecksumCalculatorThreadInfo.cpp
    glUtils.cpp)
target_compile_options(
    apigen-codec-common PRIVATE -fvisibility=hidden)
target_link_libraries(
    apigen-codec-common
    PUBLIC
    gfxstream-host-common gfxstream-base)
target_include_directories(
    apigen-codec-common
    PUBLIC
    ${GFXSTREAM_REPO_ROOT}/stream-servers/apigen-codec-common
    PRIVATE
    ${GFXSTREAM_REPO_ROOT}/stream-servers
    ${GFXSTREAM_REPO_ROOT}/include)

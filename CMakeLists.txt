project(gfx-streaming-kit)
cmake_minimum_required(VERSION 3.5)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/toolchain")

include(toolchain)

_get_host_tag(TAG)
toolchain_configure_tags(${TAG})
add_executable(buildtest test.c)

# Common base library###########################################################

add_library(
    gfxstreambase
    base/MemStream.cpp
    base/Stream.cpp
    base/StreamSerializing.cpp
    base/SubAllocator.cpp)
target_include_directories(
    gfxstreambase PRIVATE .)

# Protocols and associated code generators######################################

# Frontends#####################################################################

# Backends######################################################################

# Transport implementations#####################################################

# Test environments#############################################################

# Target: Host backend to consume real guest traffic############################

# Target: Host front+back drivers for test environment##########################

# Target: Host front+back drivers for app isolation#############################

# Unit tests####################################################################

# Functional tests##############################################################


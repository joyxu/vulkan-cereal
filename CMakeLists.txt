project(gfx-streaming-kit)
cmake_minimum_required(VERSION 3.11)
find_package(Threads)
include(ExternalProject)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)

# GoogleTest support: uses CMake ExternalProject to pull in external repo
# without manual steps (during CMake build script generation phase)
include(gtest.cmake)

set(GFXSTREAM_REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

# Third party dependencies
add_subdirectory(third-party)

# Common base libraries for host################################################

add_subdirectory(base)
add_subdirectory(snapshot)
add_subdirectory(host-common)

# Backends######################################################################

add_subdirectory(stream-servers)

# Protocols and associated code generators######################################

add_subdirectory(protocols)

# Fake Android guest#########################3##################################

add_subdirectory(fake-android-guest)

# Frontends#####################################################################

# add_subdirectory(stream-clients)

# Test environments#############################################################

# add_subdirectory(testenvs)

# Target: Host backend to consume real guest traffic############################

# Target: Host front+back drivers for test environment##########################

# Target: Host front+back drivers for app isolation#############################

# Unit tests####################################################################

# Functional tests##############################################################


project(gfx-streaming-kit)
cmake_minimum_required(VERSION 3.11)

set(CMAKE_CXX_STANDARD 17)

set(GFXSTREAM_REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

# Third party dependencies
add_subdirectory(third-party)

add_executable(buildtest test.c)

# Common base libraries for host################################################

add_subdirectory(base)
add_subdirectory(snapshot)
add_subdirectory(host-common)

# Backends######################################################################

add_subdirectory(stream-servers)

# Protocols and associated code generators######################################

add_subdirectory(protocols)

# Frontends#####################################################################

# add_subdirectory(stream-clients)

# Test environments#############################################################

# add_subdirectory(testenvs)

# Target: Host backend to consume real guest traffic############################

# Target: Host front+back drivers for test environment##########################

# Target: Host front+back drivers for app isolation#############################

# Unit tests####################################################################

# Functional tests##############################################################


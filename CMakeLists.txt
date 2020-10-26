project(gfx-streaming-kit)
cmake_minimum_required(VERSION 3.5)

add_executable(buildtest test.c)

# Common base library###########################################################

add_subdirectory(base)

# Protocols and associated code generators######################################

# add_subdirectory(protocols)

# Frontends#####################################################################

# add_subdirectory(stream-clients)

# Backends######################################################################

add_subdirectory(stream-servers)

# Transport implementations#####################################################

# add_subdirectory(transports)

# Test environments#############################################################

# add_subdirectory(testenvs)

# Target: Host backend to consume real guest traffic############################

# Target: Host front+back drivers for test environment##########################

# Target: Host front+back drivers for app isolation#############################

# Unit tests####################################################################

# Functional tests##############################################################


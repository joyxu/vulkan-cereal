add_library(
    gfxstream-host-common
    vm_operations.cpp
    feature_control.cpp)
target_include_directories(
    gfxstream-host-common PUBLIC ../)

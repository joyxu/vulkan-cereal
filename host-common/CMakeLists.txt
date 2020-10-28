add_library(
    gfxstream-host-common
    crash_reporter.cpp
    vm_operations.cpp
    feature_control.cpp)
target_include_directories(
    gfxstream-host-common PUBLIC ../)

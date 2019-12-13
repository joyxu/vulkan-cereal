add_library(
    gfxstreambase
    MemStream.cpp
    Stream.cpp
    StreamSerializing.cpp
    SubAllocator.cpp)
target_include_directories(
    gfxstreambase PUBLIC ../)

# Graphics Streaming Kit (formerly: Vulkan Cereal)

Graphics Streaming Kit is a code generator that makes it easier to serialize
and forward graphics API calls from one place to another:

- From a virtual machine guest to host for virtualized graphics
- From one process to another for IPC graphics
- From one computer to another via network sockets

# Build: Linux

Make sure the latest CMake is installed.
Make sure you are using Clang as your `CC` and `CXX`. Then

    mkdir build
    cd build
    cmake . ../
    make -j24

# Build: Windows

Make sure the latest CMake is installed.  Make sure Visual Studio 2019 is
installed on your system along with all the Clang C++ toolchain components.
Then

    mkdir build
    cd build
    cmake . ../ -A x64 -T ClangCL

A solution file should be generated. Then open the solution file in Visual
studio and build the `gfxstream_backend` target.

# Build: Android for host

Be in the Android build system. Then

    m libgfxstream_backend

It then ends up in `out/host`

# Output artifacts

    libgfxstream_backend.(dll|so|dylib)

# Tests

## Linux Tests

There are a bunch of test executables generated. They require `libEGL.so` and `libGLESv2.so` and `libvulkan.so` to be available, possibly from your GPU vendor or ANGLE, in the `$LD_LIBRARY_PATH`.

## Windows Tests

There are a bunch of test executables generated. They require `libEGL.dll` and `libGLESv2.dll` and `vulkan-1.dll` to be available, possibly from your GPU vendor or ANGLE, in the `%PATH%`.

## Android Host Tests

These are currently not built due to the dependency on system libEGL/libvulkan to run correctly.

# Structure

- `CMakeLists.txt`: specifies all host-side build targets. This includes all
  backends along with client/server setups that live only on the host. Some
  - Backend implementations
  - Implementations of the host side of various transports
  - Frontends used for host-side testing with a mock implementation of guest
    graphics stack (mainly Android)
  - Frontends that result in actual Linux/macOS/Windows gles/vk libraries
    (isolation / fault tolerance use case)
- `Android.bp`: specifies all guest-side build targets for Android:
  - Implementations of the guest side of various transports (above the kernel)
  - Frontends
- `BUILD.gn`: specifies all guest-side build targets for Fuchsia
  - Implementations of the guest side of various transports (above the kernel)
  - Frontends
- `base/`: common libraries that are built for both the guest and host.
  Contains utility code related to synchronization, threading, and suballocation.
- `protocols/`: implementations of protocols for various graphics APIs. May contain
code generators to make it easy to regen the protocol based on certain things.
- `host-common/`: implementations of host-side support code that makes it
  easier to run the server in a variety of virtual device environments.
  Contains concrete implementations of auxiliary virtual devices such as
  Address Space Device and Goldfish Pipe.
- `stream-servers/`: implementations of various backends for various graphics
  APIs that consume protocol. `gfxstream-virtio-gpu-renderer.cpp` contains a
  virtio-gpu backend implementation.

# Graphics Streaming Kit (formerly: Vulkan Cereal)

Graphics Streaming Kit is a code generator that makes it easier to serialize
and forward graphics API calls from one place to another:

- From a virtual machine guest to host for virtualized graphics
- From one process to another for IPC graphics
- From one computer to another via network sockets

# Build

Run `./build-host.sh`, or:

    mkdir build
    cd build
    cmake . ../ -DCMAKE_TOOLCHAIN_FILE=../toolchain/toolchain-linux-x86_64.cmake
    # Or toolchain-darwin, or toolchain-windows_msvc depending on host platform
    make -j24

TODO: guest build makefiles (Android.bp)

# Project layout and overall evolution plan

For fast iteration, to start with, this project will reference code from the
following projects:

    device/generic/goldfish-opengl # master branch
    platform/external/qemu/ # emu-master-dev branch

Once the minimum set of code dependencies is determined, they will be extracted
out to this project.  We'll also use those to get the initial version of the
code working on both Cuttlefish and Goldfish.

After this extraction and verification step, all the needed code for gfx
streaming kit will be contained in this project, but the toolchain prebuilts
for host side still need to be included as other projects. Therefore, we're
looking at creating a new repo branch that encompasses this project and the
toolchain prebuilts (or any other relevant projects). Thus it's a good chance
to rename this project to something more appropriate like
`device/generic/gfxstream`.

Then, we add a new go/ab target that builds + runs any relevant tests.

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
- `stream-clients/`: implementations of various frontends for various graphics
  APIs that generate protocol.
- `stream-servers/`: implementations of various backends for various graphics
  APIs that consume protocol.
- `toolchain/`: includes various CMake toolchain files for the host-side build
- `transports/`: libraries that live on both guest and host that implement
  various transports.  Does not care about what data is passed through, only
  how.
- `testenvs/`: includes host-side mock implementations of guest graphics stacks,
incl. Android
- `tests/`: includes functional tests use a mock transport and test environment

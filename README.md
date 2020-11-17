# Vulkan Cereal

Vulkan Cereal is a code generator that makes it easier to serialize and forward
Vulkan API calls from one place to another, such as:

- From a virtual machine guest to host for virtualized graphics
- From one process to another for IPC graphics
- From one computer to another via network sockets

# Installation, build, and Hello World use case

Currently, Vulkan Cereal is written in Python as it is a well-known language
with batteries included that is easy to edit by many developers.  However, it's
possible this can change in the future to an Ocaml / Haskell DSL as it will be
much easier to specify generic and correct code generators.

## Installation

Vulkan Cereal requires Python 3+ along with lxml for XML parsing.

## Hello World (TODO: Build hello world example)

    python cereal.py [path-to-vk.xml] [target-module.json]

# Terminology

We realize that due to the broad set of possible use cases,
it can get confusing. We choose to go with the terms "guest" and "host" for now
as it reflects the current actual usage the best.

- Guest: the user of the Vulkan API, aka 'vm guest', 'client', 'local', etc.
- Host: the place where the Vulkan driver actually lives, aka 'vm host', 'server', 'remote', etc.

# Input

Cereal takes as input a Vulkan specification in the form of an XML document
along with a _target_ module that consists of:

- Target language
    - Tells cereal what language is used to generate code. Currently, only C++ is supported.
- Stream api definition
    - Tells cereal what code to generate when sending bytes to and from the host.
    - write()
    - read()
    - flush()
- Vulkan handle mappings
    - create mapping: what code to call when a new Vulkan object is created in the guest and how to get the new handle.
    - wrap/unwrap: what code to call when we need to wrap or otherwise access the host version of the Vulkan object.
- API exceptions
    - no-streaming exception: APIs that are handled completely on one end or the other without serialization.
    - exception: APIs that are still streamed in some way, but pre-processed in a separate class, so that
      the entry point points to the preprocessor, not directly to the streaming version.

We will include some widely-used target modules as examples.
Target modules are specified in .json format.

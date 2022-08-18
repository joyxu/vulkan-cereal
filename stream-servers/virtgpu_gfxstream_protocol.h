// Copyright 2022 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef VIRTGPU_GFXSTREAM_PROTOCOL_H
#define VIRTGPU_GFXSTREAM_PROTOCOL_H

#include <stdint.h>

// Address Space Graphics contexts
#define GFXSTREAM_CONTEXT_CREATE                0x1001
#define GFXSTREAM_CONTEXT_PING                  0x1002
#define GFXSTREAM_CONTEXT_PING_WITH_RESPONSE    0x1003

// Native Sync FD
#define GFXSTREAM_CREATE_EXPORT_SYNC            0x9000
#define GFXSTREAM_CREATE_IMPORT_SYNC            0x9001

// Vulkan Sync
#define GFXSTREAM_CREATE_EXPORT_SYNC_VK         0xa000
#define GFXSTREAM_CREATE_IMPORT_SYNC_VK         0xa001
#define GFXSTREAM_CREATE_QSRI_EXPORT_VK         0xa002

struct gfxstreamHeader {
    uint32_t opCode;
};

struct gfxstreamContextCreate {
    struct gfxstreamHeader hdr;
    uint32_t deviceType;
};

struct gfxstreamContextPing {
    struct gfxstreamHeader hdr;
    uint32_t phys_addr_lo;
    uint32_t phys_addr_hi;
    uint32_t size_lo;
    uint32_t size_hi;
    uint32_t metadata_lo;
    uint32_t metadata_hi;
    uint32_t wait_phys_addr_lo;
    uint32_t wait_phys_addr_hi;
    uint32_t wait_flags;
    uint32_t direction;
};

struct gfxstreamContextPingWithResponse {
    struct gfxstreamHeader hdr;
    uint32_t resp_resid;
    uint32_t phys_addr_lo;
    uint32_t phys_addr_hi;
    uint32_t size_lo;
    uint32_t size_hi;
    uint32_t metadata_lo;
    uint32_t metadata_hi;
    uint32_t wait_phys_addr_lo;
    uint32_t wait_phys_addr_hi;
    uint32_t wait_flags;
    uint32_t direction;
};

struct gfxstreamCreateExportSync {
    struct gfxstreamHeader hdr;
    uint32_t syncHandleLo;
    uint32_t syncHandleHi;
};

struct gfxstreamCreateExportSyncVK {
    struct gfxstreamHeader hdr;
    uint32_t deviceHandleLo;
    uint32_t deviceHandleHi;
    uint32_t fenceHandleLo;
    uint32_t fenceHandleHi;
};

struct gfxstreamCreateQSRIExportVK {
    struct gfxstreamHeader hdr;
    uint32_t imageHandleLo;
    uint32_t imageHandleHi;
};

#endif

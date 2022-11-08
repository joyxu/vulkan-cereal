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
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either expresso or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <memory>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES/gl.h>
#include <GLES3/gl3.h>

#include "ContextHelper.h"
#include "Handle.h"
#include "snapshot/LazySnapshotObj.h"

class Buffer : public android::snapshot::LazySnapshotObj<Buffer> {
  public:
    static Buffer* create(size_t sizeBytes, HandleType hndl, ContextHelper* helper);

    ~Buffer() = default;

    HandleType getHndl() const { return m_handle; }
    size_t getSize() const { return m_sizeBytes; }

    void read(uint64_t offset, uint64_t size, void* bytes);

    void subUpdate(uint64_t offset, uint64_t size, void* bytes);

  protected:
    Buffer(size_t sizeBytes, HandleType hndl, ContextHelper* helper)
        : m_handle(hndl), m_sizeBytes(sizeBytes), m_helper(helper) {}

  private:
    /*
    // TODO: GL_EXT_external_buffer
    GLuint m_buffer = 0;
    */
    HandleType m_handle;
    size_t m_sizeBytes;
    ContextHelper* m_helper = nullptr;
};

typedef std::shared_ptr<Buffer> BufferPtr;

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

class TextureDraw;

// ContextHelper interface class used during Buffer and ColorBuffer
// operations. This is introduced to remove coupling from the FrameBuffer
// class implementation.
class ContextHelper {
  public:
    ContextHelper() = default;
    virtual ~ContextHelper() = default;
    virtual bool setupContext() = 0;
    virtual void teardownContext() = 0;
    virtual TextureDraw* getTextureDraw() const = 0;
    virtual bool isBound() const = 0;
};

// Helper class to use a ContextHelper for some set of operations.
// Usage is pretty simple:
//
//     {
//        RecursiveScopedContextBind context(m_helper);
//        if (!context.isOk()) {
//            return false;   // something bad happened.
//        }
//        .... do something ....
//     }   // automatically calls m_helper->teardownContext();
//
class RecursiveScopedContextBind {
  public:
    RecursiveScopedContextBind(ContextHelper* helper) : mHelper(helper) {
        if (helper->isBound()) return;
        if (!helper->setupContext()) {
            mHelper = NULL;
            return;
        }
        mNeedUnbind = true;
    }

    bool isOk() const { return mHelper != NULL; }

    ~RecursiveScopedContextBind() { release(); }

    void release() {
        if (mNeedUnbind) {
            mHelper->teardownContext();
            mNeedUnbind = false;
        }
        mHelper = NULL;
    }

  private:
    ContextHelper* mHelper;
    bool mNeedUnbind = false;
};

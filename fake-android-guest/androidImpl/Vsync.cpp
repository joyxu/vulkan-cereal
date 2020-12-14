// Copyright 2018 The Android Open Source Project
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
#include "Vsync.h"

#include "base/ConditionVariable.h"
#include "base/Lock.h"
#include "base/System.h"
#include "base/FunctorThread.h"

#include <atomic>
#include <memory>

using android::base::AutoLock;
using android::base::ConditionVariable;
using android::base::FunctorThread;
using android::base::Lock;

namespace aemu {

class Vsync::Impl {
public:
    Impl(Callback callback, int refreshRate = 60)
        : mCallback(std::move(callback)),
          mRefreshRate(refreshRate),
          mRefreshIntervalUs(1000000ULL / mRefreshRate),
          mThread([this] {

              mNowUs = android::base::getHighResTimeUs();
              mNextVsyncDeadlineUs = mNowUs + mRefreshIntervalUs;

              while (true) {
                  if (mShouldStop.load(std::memory_order_relaxed))
                      return 0;

                  mNowUs = android::base::getHighResTimeUs();

                  if (mNextVsyncDeadlineUs > mNowUs) {
                    android::base::sleepUs(mNextVsyncDeadlineUs - mNowUs);
                  }

                  mNowUs = android::base::getHighResTimeUs();
                  mNextVsyncDeadlineUs = mNowUs + mRefreshIntervalUs;

                  AutoLock lock(mLock);
                  mSync = 1;
                  mCv.signal();
                  mCallback();
              }
              return 0;
          }) {
    }

    void start() {
        mShouldStop.store(false, std::memory_order_relaxed);
        mThread.start();
    }

    void join() {
        mShouldStop.store(true, std::memory_order_relaxed);
        mThread.wait();
    }

    ~Impl() { join(); }

    void waitUntilNextVsync() {
        AutoLock lock(mLock);
        mSync = 0;
        while (!mSync) {
            mCv.wait(&mLock);
        }
    }

private:
    std::atomic<bool> mShouldStop { false };
    int mSync = 0;
    Lock mLock;
    ConditionVariable mCv;

    Callback mCallback;
    int mRefreshRate = 60;
    uint64_t mRefreshIntervalUs;
    uint64_t mNowUs = 0;
    uint64_t mNextVsyncDeadlineUs = 0;
    FunctorThread mThread;
};

Vsync::Vsync(int refreshRate, Vsync::Callback callback)
    : mImpl(new Vsync::Impl(std::move(callback), refreshRate)) {}

Vsync::~Vsync() = default;

void Vsync::start() { mImpl->start(); }
void Vsync::join() { mImpl->join(); }

void Vsync::waitUntilNextVsync() {
    mImpl->waitUntilNextVsync();
}

} // namespace aemu

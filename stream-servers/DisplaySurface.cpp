// Copyright (C) 2022 The Android Open Source Project
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

#include "DisplaySurface.h"

#include "Display.h"
#include "host-common/GfxstreamFatalError.h"
#include "host-common/logging.h"

namespace gfxstream {

using emugl::ABORT_REASON_OTHER;
using emugl::FatalError;

DisplaySurface::DisplaySurface(uint32_t width,
                               uint32_t height,
                               std::unique_ptr<DisplaySurfaceImpl> impl)
    : mWidth(width),
      mHeight(height),
      mImpl(std::move(impl)) {}

DisplaySurface::~DisplaySurface() {
    if (!mBoundUsers.empty()) {
        GFXSTREAM_ABORT(FatalError(ABORT_REASON_OTHER))
            << "DisplaySurface destroyed while there are still users!";
    }
}

uint32_t DisplaySurface::getWidth() const {
    std::lock_guard<std::mutex> lock(mParamsMutex);
    return mWidth;
}

uint32_t DisplaySurface::getHeight() const {
    std::lock_guard<std::mutex> lock(mParamsMutex);
    return mHeight;
}

void DisplaySurface::updateSize(uint32_t newWidth, uint32_t newHeight) {
    std::lock_guard<std::mutex> lock(mParamsMutex);
    mWidth = newHeight;
    mHeight = newHeight;
}

void DisplaySurface::registerUser(DisplaySurfaceUser* user) {
    mBoundUsers.insert(user);
}

void DisplaySurface::unregisterUser(DisplaySurfaceUser* user) {
    mBoundUsers.erase(user);
}

}  // namespace gfxstream

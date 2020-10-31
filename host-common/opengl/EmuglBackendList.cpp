// Copyright 2015 The Android Open Source Project
//
// This software is licensed under the terms of the GNU General Public
// License version 2, as published by the Free Software Foundation, and
// may be copied, distributed, and modified under those terms.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

#include "EmuglBackendList.h"

#include "base/StringFormat.h"
#include "base/System.h"
#include "base/PathUtils.h"

#define DEBUG 0

#if DEBUG
#  include <stdio.h>
#  define D(...)  printf(__VA_ARGS__)
#else
#  define D(...)  ((void)0)
#endif

namespace android {
namespace opengl {

EmuglBackendList::EmuglBackendList(int programBitness,
                                   const std::vector<std::string>& names) :
        mDefaultName("auto"), mNames(names), mProgramBitness(programBitness) { }

bool EmuglBackendList::contains(const char* name) const {
    for (size_t n = 0; n < mNames.size(); ++n) {
        if (mNames[n] == name) {
            return true;
        }
    }
    return false;
}

std::string EmuglBackendList::getLibDirPath(const char* name) {
    // remove the "_indirect" suffix
    std::string suffix("_indirect");
    std::string nameNoSuffix(name);
    int nameNoSuffixLen = (int)nameNoSuffix.size() - (int)suffix.size();
    if (nameNoSuffixLen > 0 &&
        suffix == nameNoSuffix.c_str() + nameNoSuffixLen) {
        nameNoSuffix.erase(nameNoSuffixLen);
    }
    return android::base::pj({mExecDir, "lib64", std::string("gles_%s") + nameNoSuffix});
}

#ifdef _WIN32
static const char kLibSuffix[] = ".dll";
#elif defined(__APPLE__)
static const char kLibSuffix[] = ".dylib";
#else
static const char kLibSuffix[] = ".so";
#endif

bool EmuglBackendList::getBackendLibPath(const char* name,
                                         Library library,
                                         std::string* libPath) {

    const char* libraryName = NULL;
    if (library == LIBRARY_EGL) {
        libraryName = "EGL";
    } else if (library == LIBRARY_GLESv1) {
        libraryName = "GLES_CM";
    } else if (library == LIBRARY_GLESv2) {
        libraryName = "GLESv2";
    } else {
        // Should not happen.
        D("%s: Invalid library type: %d\n", __FUNCTION__, library);
        return false;
    }

    std::string path = android::base::pj({
            getLibDirPath(name), std::string("lib") + libraryName + kLibSuffix});

    *libPath = path;
    return true;
}

}  // namespace opengl
}  // namespace android

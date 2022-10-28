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

#include <gmock/gmock.h>

#include "CompressedImageInfo.h"

namespace goldfish_vk {
namespace {

using ::testing::Eq;

TEST(CompressedImageInfo, CreateAstc) {
    CompressedImageInfo cmpInfo = CompressedImageInfo::create(VK_FORMAT_ASTC_10x5_UNORM_BLOCK);
    EXPECT_THAT(cmpInfo.isCompressed, Eq(true));
    EXPECT_THAT(cmpInfo.isEtc2, Eq(false));
    EXPECT_THAT(cmpInfo.isAstc, Eq(true));
    EXPECT_THAT(cmpInfo.compFormat, Eq(VK_FORMAT_ASTC_10x5_UNORM_BLOCK));
    EXPECT_THAT(cmpInfo.decompFormat, Eq(VK_FORMAT_R8G8B8A8_UNORM));
    EXPECT_THAT(cmpInfo.sizeCompFormat, Eq(VK_FORMAT_R32G32B32A32_UINT));
    EXPECT_THAT(cmpInfo.compressedBlockWidth, Eq(10));
    EXPECT_THAT(cmpInfo.compressedBlockHeight, Eq(5));
}

TEST(CompressedImageInfo, CreateEtc2) {
    CompressedImageInfo cmpInfo = CompressedImageInfo::create(VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK);
    EXPECT_THAT(cmpInfo.isCompressed, Eq(true));
    EXPECT_THAT(cmpInfo.isEtc2, Eq(true));
    EXPECT_THAT(cmpInfo.isAstc, Eq(false));
    EXPECT_THAT(cmpInfo.compFormat, Eq(VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK));
    EXPECT_THAT(cmpInfo.decompFormat, Eq(VK_FORMAT_R8G8B8A8_SRGB));
    EXPECT_THAT(cmpInfo.sizeCompFormat, Eq(VK_FORMAT_R16G16B16A16_UINT));
    EXPECT_THAT(cmpInfo.compressedBlockWidth, Eq(4));
    EXPECT_THAT(cmpInfo.compressedBlockHeight, Eq(4));
}

TEST(CompressedImageInfo, CreateNonCompressed) {
    CompressedImageInfo cmpInfo = CompressedImageInfo::create(VK_FORMAT_R8G8B8A8_SRGB);
    EXPECT_THAT(cmpInfo.isCompressed, Eq(false));
    EXPECT_THAT(cmpInfo.isEtc2, Eq(false));
    EXPECT_THAT(cmpInfo.isAstc, Eq(false));
    EXPECT_THAT(cmpInfo.compFormat, Eq(VK_FORMAT_R8G8B8A8_SRGB));
    EXPECT_THAT(cmpInfo.decompFormat, Eq(VK_FORMAT_R8G8B8A8_SRGB));
    EXPECT_THAT(cmpInfo.sizeCompFormat, Eq(VK_FORMAT_R8G8B8A8_SRGB));
    EXPECT_THAT(cmpInfo.compressedBlockWidth, Eq(1));
    EXPECT_THAT(cmpInfo.compressedBlockHeight, Eq(1));
}

}  // namespace
}  // namespace goldfish_vk
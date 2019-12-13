#!/bin/sh

# Copyright 2019 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

. $(dirname "$0")/utils/common.shi

shell_import utils/aosp_dir.shi

PROGRAM_PARAMETERS=""

PROGRAM_DESCRIPTION=\
"Regenerate gfxstream Vulkan sources."

option_register_var "--only=<module-name>" OPT_ONLY "Only update a particular generated module"

aosp_dir_register_option

option_parse "$@"

aosp_dir_parse_option

REPO_DIR=$AOSP_DIR/device/generic/vulkan-cereal
VULKAN_REGISTRY_XML_DIR=$REPO_DIR/protocols/vk-gen/xml
VULKAN_SRC_DIR=$REPO_DIR/stream-servers/vulkan
CEREAL_OUTPUT_DIR=$VULKAN_SRC_DIR/cereal

export VK_CEREAL_GUEST_ENCODER_DIR=$REPO_DIR/stream-clients/vulkan_enc
export VK_CEREAL_GUEST_HAL_DIR=$REPO_DIR/stream-clients/vulkan
export VK_CEREAL_HOST_DECODER_DIR=$REPO_DIR/stream-servers/vulkan
export VK_CEREAL_HOST_PROTO_DIR=$REPO_DIR/stream-servers/vulkan/cereal/proto
export VK_CEREAL_HOST_INCLUDE_DIR=$REPO_DIR/include
export VK_CEREAL_BASELIB_PREFIX=base
export VK_CEREAL_BASELIB_LINKNAME=gfxstreambase

mkdir -p $CEREAL_OUTPUT_DIR

if [ "$OPT_ONLY" ]; then
export ANDROID_EMU_VK_CEREAL_SUPPRESS=$OPT_ONLY
fi

python3 $VULKAN_REGISTRY_XML_DIR/genvk.py -registry $VULKAN_REGISTRY_XML_DIR/vk.xml cereal -o $CEREAL_OUTPUT_DIR

log "Done generating Vulkan driver."


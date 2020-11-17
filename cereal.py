# Copyright (c) 2019 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
import sys
import xml.etree.ElementTree as etree

xmlFileName = sys.argv[1]
targetFileName = sys.argv[2]

# Parse vk.xml, get information at the XML level.

xmlTree = etree.parse(xmlFileName)
xmlRoot = xmlTree.getroot()

vulkanRegistryElements = [
    "types/type",
    "enums",
    "commands/command",
    "feature",
    "extensions/extension",
]

vulkanRegistryDicts = {}

def findToplevelRegistryElements(key):
    orderedKeys = []
    elementDict = {}
    for e in xmlRoot.findall(key):
        if "api" in e.attrib:
            subKey = (e.get("name"),e.get("api"))
        else:
            subKey = e.get("name")
        if subKey == None:
            if e.find("proto/name") is not None:
                subKey = e.find("proto/name").text
            if subKey == None:
                subKey = e.find("name").text
        orderedKeys.append(subKey)
        elementDict[subKey] = e
    return (orderedKeys, elementDict)
    
vulkanRegistryDicts = \
    dict(map( \
        lambda k: (k, findToplevelRegistryElements(k)),
            vulkanRegistryElements))

# Transform the resulting info to info about Vulkan types
# or APIs in general.
# for k in vulkanRegistryElements:
#     print k
#     for e in vulkanRegistryDicts[k][0]:
#         print e

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

from cereal.common.vulkantypes import VulkanTypeInfo

def findToplevelRegistryElements(xmlRoot, key):
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

def initElementNameDict():
    d = { "types" : [], "enums" : [], "commands" : [], }
    return d

def populateRequiredElementNames(commandXmlDict, featureOrExtensionXmlNode, dictionary):
    for elts in featureOrExtensionXmlNode.findall("require"):
        for t in elts.findall("type"):
            dictionary["types"].append(t.get("name"))
        for t in elts.findall("enum"):
            dictionary["enums"].append(t.get("name"))
        for t in elts.findall("command"):
            dictionary["commands"].append(t.get("name"))
            for t_dependent in commandXmlDict[t.get("name")].findall(".//type"):
                dictionary["types"].append(t_dependent.text)

def populateRequiredNamesFromRegistryDict(commandXmlDict, registryDict, registryKey):
    res = {}
    for k in registryDict[registryKey][0]:
        res[k] = initElementNameDict()
        populateRequiredElementNames( \
            commandXmlDict,
            registryDict[registryKey][1][k],
            res[k])
    return res

def iterateCereal(registryDict, cereal):
    visited = {
        "types" : set(),
        "enums" : set(),
        "commands" : set(),
    }

if __name__ == "__main__":
    xmlFileName = sys.argv[1]
    targetFileName = sys.argv[2]

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

    vulkanRegistryDicts = \
        dict(map( \
            lambda k: (k, findToplevelRegistryElements(xmlRoot, k)),
                vulkanRegistryElements))

    commandXmlDict = vulkanRegistryDicts["commands/command"][1]

    requiredNamesCore = \
        populateRequiredNamesFromRegistryDict(commandXmlDict, vulkanRegistryDicts, "feature")
    requiredNamesExtensions = \
        populateRequiredNamesFromRegistryDict(commandXmlDict, vulkanRegistryDicts, "extensions/extension")



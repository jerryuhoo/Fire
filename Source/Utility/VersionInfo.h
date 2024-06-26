/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2020 - Raw Material Software Limited

   JUCE is an open source library subject to commercial or open-source
   licensing.

   By using JUCE, you agree to the terms of both the JUCE 6 End-User License
   Agreement and JUCE Privacy Policy (both effective as of the 16th June 2020).

   End User License Agreement: www.juce.com/juce-6-licence
   Privacy Policy: www.juce.com/juce-privacy-policy

   Or: You may also use this code under the terms of the GPL v3 (see
   www.gnu.org/licenses).

   JUCE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

  ==============================================================================
*/

#pragma once
#include "juce_core/juce_core.h"
#include "../GUI/InterfaceDefines.h"

//==============================================================================
class VersionInfo
{
public:
    struct Asset
    {
        const juce::String name;
        const juce::String url;
    };

    static std::unique_ptr<VersionInfo> fetchFromUpdateServer (const juce::String& versionString);
    static std::unique_ptr<VersionInfo> fetchLatestFromUpdateServer();
    static std::unique_ptr<juce::InputStream> createInputStreamForAsset (const Asset& asset, int& statusCode);

    bool isNewerVersionThanCurrent();

    const juce::String versionString;
    const juce::String releaseNotes;
    const std::vector<Asset> assets;

private:
    VersionInfo (juce::String version, juce::String releaseNotes, std::vector<Asset> assets);

    static std::unique_ptr<VersionInfo> fetch (const juce::String&);
};

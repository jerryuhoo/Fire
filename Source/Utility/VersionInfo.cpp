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


#include "VersionInfo.h"


VersionInfo::VersionInfo (juce::String versionIn, juce::String releaseNotesIn, std::vector<Asset> assetsIn)
    : versionString (std::move (versionIn)),
      releaseNotes (std::move (releaseNotesIn)),
      assets (std::move (assetsIn))
{}

std::unique_ptr<VersionInfo> VersionInfo::fetchFromUpdateServer (const juce::String& versionString)
{
    return fetch ("tags/" + versionString);
}

std::unique_ptr<VersionInfo> VersionInfo::fetchLatestFromUpdateServer()
{
    return fetch ("latest");
}

std::unique_ptr<juce::InputStream> VersionInfo::createInputStreamForAsset (const Asset& asset, int& statusCode)
{
    juce::URL downloadUrl (asset.url);
    juce::StringPairArray responseHeaders;

    return std::unique_ptr<juce::InputStream> (downloadUrl.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress).withExtraHeaders ("Accept: application/octet-stream")
                                                 .withConnectionTimeoutMs (5000)
                                                 .withResponseHeaders (&responseHeaders)
                                                 .withStatusCode (&statusCode)
                                                 .withNumRedirectsToFollow (1)));
}

bool VersionInfo::isNewerVersionThanCurrent()
{
    jassert (versionString.isNotEmpty());

    auto currentTokens = juce::StringArray::fromTokens (VERSION, ".", {});
    auto thisTokens    = juce::StringArray::fromTokens (versionString, ".", {});

    jassert (thisTokens.size() == 3);

    if (currentTokens[0].getIntValue() == thisTokens[0].getIntValue())
    {
        if (currentTokens[1].getIntValue() == thisTokens[1].getIntValue())
            return currentTokens[2].getIntValue() < thisTokens[2].getIntValue();

        return currentTokens[1].getIntValue() < thisTokens[1].getIntValue();
    }

    return currentTokens[0].getIntValue() < thisTokens[0].getIntValue();
}

std::unique_ptr<VersionInfo> VersionInfo::fetch (const juce::String& endpoint)
{
    juce::URL latestVersionURL ("https://api.github.com/repos/jerryuhoo/Fire/releases/" + endpoint);
    std::unique_ptr<juce::InputStream> inStream (latestVersionURL.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress).withConnectionTimeoutMs (5000)));

    if (inStream == nullptr)
        return nullptr;

    auto content = inStream->readEntireStreamAsString();
    auto latestReleaseDetails = juce::JSON::parse (content);

    auto* json = latestReleaseDetails.getDynamicObject();

    if (json == nullptr)
        return nullptr;

    auto versionString = json->getProperty ("tag_name").toString();

    if (versionString.isEmpty())
        return nullptr;

    auto* assets = json->getProperty ("assets").getArray();

    if (assets == nullptr)
        return nullptr;

    auto releaseNotes = json->getProperty ("body").toString();
    std::vector<VersionInfo::Asset> parsedAssets;

    for (auto& asset : *assets)
    {
        if (auto* assetJson = asset.getDynamicObject())
        {
            parsedAssets.push_back ({ assetJson->getProperty ("name").toString(),
                                      assetJson->getProperty ("url").toString() });
            jassert (parsedAssets.back().name.isNotEmpty());
            jassert (parsedAssets.back().url.isNotEmpty());
        }
        else
        {
            jassertfalse;
        }
    }

    return std::unique_ptr<VersionInfo> (new VersionInfo { versionString, releaseNotes, std::move (parsedAssets) });
}

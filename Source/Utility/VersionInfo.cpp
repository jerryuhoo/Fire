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
#include <array>

namespace
{
bool parseVersionComponents(juce::String version, std::array<int, 3>& components)
{
    version = version.trim();
    if (version.startsWithIgnoreCase("v"))
        version = version.substring(1);

    version = version.upToFirstOccurrenceOf("+", false, false)
                  .upToFirstOccurrenceOf("-", false, false);
    const auto tokens = juce::StringArray::fromTokens(version, ".", {});
    if (tokens.size() != static_cast<int>(components.size()))
        return false;

    for (int i = 0; i < tokens.size(); ++i)
    {
        const auto token = tokens[i].trim();
        if (token.isEmpty() || ! token.containsOnly("0123456789"))
            return false;

        components[static_cast<size_t>(i)] = juce::jmax(0, token.getIntValue());
    }

    return true;
}
} // namespace


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

bool VersionInfo::isNewerVersionThanCurrent() const
{
    std::array<int, 3> currentVersion {};
    std::array<int, 3> fetchedVersion {};
    if (! parseVersionComponents(VERSION, currentVersion)
        || ! parseVersionComponents(versionString, fetchedVersion))
        return false;

    return fetchedVersion > currentVersion;
}

std::unique_ptr<VersionInfo> VersionInfo::fetch (const juce::String& endpoint)
{
    FetchOperation operation;
    return endpoint == "latest" ? operation.fetchLatest()
                                : operation.fetchVersion(endpoint.fromFirstOccurrenceOf("tags/", false, false));
}

std::unique_ptr<VersionInfo> VersionInfo::FetchOperation::fetchLatest()
{
    return fetchEndpoint("latest");
}

std::unique_ptr<VersionInfo> VersionInfo::FetchOperation::fetchVersion(const juce::String& versionString)
{
    return fetchEndpoint("tags/" + versionString);
}

void VersionInfo::FetchOperation::reset()
{
    const juce::ScopedLock lock(streamLock);
    jassert(activeStream == nullptr);
    cancelled.store(false, std::memory_order_release);
}

void VersionInfo::FetchOperation::cancel()
{
    cancelled.store(true, std::memory_order_release);

    std::shared_ptr<juce::WebInputStream> stream;
    {
        const juce::ScopedLock lock(streamLock);
        stream = activeStream;
    }

    if (stream != nullptr)
        stream->cancel();
}

std::unique_ptr<VersionInfo> VersionInfo::FetchOperation::fetchEndpoint(const juce::String& endpoint)
{
    if (cancelled.load(std::memory_order_acquire))
        return nullptr;

    juce::URL latestVersionURL ("https://api.github.com/repos/jerryuhoo/Fire/releases/" + endpoint);
    auto stream = std::make_shared<juce::WebInputStream>(latestVersionURL, false);
    stream->withConnectionTimeout(5000)
        .withNumRedirectsToFollow(1)
        .withExtraHeaders("Accept: application/vnd.github+json\r\n"
                          "User-Agent: Fire-Audio-Plugin\r\n");

    {
        const juce::ScopedLock lock(streamLock);
        if (cancelled.load(std::memory_order_acquire))
            return nullptr;
        activeStream = stream;
    }

    const auto clearActiveStream = [&]
    {
        const juce::ScopedLock lock(streamLock);
        if (activeStream == stream)
            activeStream.reset();
    };

    if (! stream->connect(nullptr)
        || stream->getStatusCode() < 200
        || stream->getStatusCode() >= 300)
    {
        clearActiveStream();
        return nullptr;
    }

    constexpr size_t maximumResponseBytes = 4u * 1024u * 1024u;
    juce::MemoryOutputStream response;
    std::array<char, 8192> readBuffer {};

    while (! cancelled.load(std::memory_order_acquire) && ! stream->isExhausted())
    {
        const auto remaining = maximumResponseBytes - response.getDataSize();
        if (remaining == 0)
            break;

        const int bytesRead = stream->read(readBuffer.data(),
                                           static_cast<int>(juce::jmin(remaining, readBuffer.size())));
        if (bytesRead <= 0)
            break;
        response.write(readBuffer.data(), static_cast<size_t>(bytesRead));
    }

    const bool wasCancelled = cancelled.load(std::memory_order_acquire);
    const bool responseTooLarge = ! wasCancelled
                                  && response.getDataSize() >= maximumResponseBytes
                                  && ! stream->isExhausted();
    clearActiveStream();

    if (wasCancelled || responseTooLarge || response.getDataSize() == 0)
        return nullptr;

    const auto content = juce::String::fromUTF8(static_cast<const char*>(response.getData()),
                                                 static_cast<int>(response.getDataSize()));
    return VersionInfo::parseReleaseResponse(content);
}

std::unique_ptr<VersionInfo> VersionInfo::parseReleaseResponse(const juce::String& content)
{
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

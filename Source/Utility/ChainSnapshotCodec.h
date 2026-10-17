#pragma once
#include "ChainSnapshot.h"

namespace fire::chain
{
inline constexpr int schemaVersion = 1;
inline constexpr int maximumModules = 13;
inline constexpr int maximumRoutes = 128;
inline constexpr juce::int64 maximumFileBytes = 10 * 1024 * 1024;

// Message-thread file/clipboard operations. Failed decoding or file reads
// leave the destination snapshot unchanged.
std::unique_ptr<juce::XmlElement> encode(const Snapshot&);
juce::Result decode(const juce::XmlElement&, Snapshot&);
juce::Result read(const juce::File&, Snapshot&);
juce::Result write(const juce::File&, const Snapshot&);
juce::Result setClipboard(ClipboardKind, const Snapshot&);
std::shared_ptr<const Snapshot> getClipboard(ClipboardKind);
}

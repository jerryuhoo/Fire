#pragma once
#include "../DSP/Clouds/FrozenRecording.h"
#include <juce_core/juce_core.h>

namespace fire::effects
{
inline constexpr size_t frozenRecordingBytes = 16u + static_cast<size_t>(frozenRecordingFrames) * 4u;
inline std::uint32_t frozenAudioChecksum(const void* data, size_t size) noexcept
{
    auto* bytes = static_cast<const unsigned char*>(data);
    std::uint32_t hash = 2166136261u;
    for (size_t i = 0; i < size; ++i) hash = (hash ^ bytes[i]) * 16777619u;
    return hash;
}
inline juce::String encodeFrozenRecording(const FrozenRecording& recording)
{
    juce::MemoryOutputStream raw;
    raw.writeInt(0x46525a31);
    raw.writeInt(recording.head);
    raw.writeInt(recording.validFrames);
    for (const auto& channel : recording.samples)
        for (auto sample : channel) raw.writeShort(sample);
    raw.writeInt(static_cast<int>(frozenAudioChecksum(raw.getData(), raw.getDataSize())));
    juce::MemoryOutputStream packed;
    { juce::GZIPCompressorOutputStream compress(packed, 6); compress.write(raw.getData(), raw.getDataSize()); }
    juce::MemoryOutputStream packet;
    packet.writeInt(0x465a5031);
    packet.writeInt(static_cast<int>(packed.getDataSize()));
    packet.writeInt(static_cast<int>(frozenAudioChecksum(packed.getData(), packed.getDataSize())));
    packet.write(packed.getData(), packed.getDataSize());
    return juce::Base64::toBase64(packet.getData(), packet.getDataSize());
}
inline FrozenRecordingPtr decodeFrozenRecording(const juce::String& encoded)
{
    if (encoded.isEmpty() || encoded.length() > 200000) return {};
    juce::MemoryOutputStream packed;
    if (!juce::Base64::convertFromBase64(packed, encoded) || packed.getDataSize() > 150000) return {};
    if (packed.getDataSize() < 12) return {};
    juce::MemoryInputStream packet(packed.getData(), packed.getDataSize(), false);
    if (packet.readInt() != 0x465a5031) return {};
    const auto length = packet.readInt();
    const auto checksum = static_cast<std::uint32_t>(packet.readInt());
    if (length < 0 || static_cast<size_t>(length) != packed.getDataSize() - 12u) return {};
    const auto* compressed = static_cast<const char*>(packed.getData()) + 12;
    if (checksum != frozenAudioChecksum(compressed, static_cast<size_t>(length))) return {};
    juce::MemoryInputStream input(compressed, static_cast<size_t>(length), false);
    juce::GZIPDecompressorInputStream decompress(input);
    juce::MemoryBlock bytes(frozenRecordingBytes + 1u);
    size_t read = 0;
    while (read < bytes.getSize())
    {
        const auto count = decompress.read(static_cast<char*>(bytes.getData()) + read,
                                          static_cast<int>(bytes.getSize() - read));
        if (count <= 0) break;
        read += static_cast<size_t>(count);
    }
    if (read != frozenRecordingBytes) return {};
    juce::MemoryInputStream raw(bytes.getData(), read, false);
    if (raw.readInt() != 0x46525a31) return {};
    auto recording = std::make_shared<FrozenRecording>();
    recording->head = raw.readInt(); recording->validFrames = raw.readInt();
    if (!recording->isValid()) return {};
    for (auto& channel : recording->samples) for (auto& sample : channel) sample = raw.readShort();
    if (static_cast<std::uint32_t>(raw.readInt()) != frozenAudioChecksum(bytes.getData(), frozenRecordingBytes - 4u)) return {};
    return recording;
}
inline bool readFrozenAudioState(const juce::XmlElement& root, FrozenRecordings& state)
{
    state = {};
    const auto* media = root.getChildByName("FROZEN_AUDIO");
    if (media == nullptr) return true;
    if (media->getStringAttribute("version") != "1" || media->getNumChildElements() > 40) return false;
    int parents = 0;
    for (auto* child : root.getChildIterator()) if (child->hasTagName("FROZEN_AUDIO")) ++parents;
    if (parents != 1) return false;
    for (auto* child : media->getChildIterator())
    {
        const auto scopeText = child->getStringAttribute("scope"), slotText = child->getStringAttribute("slot");
        if (!child->hasTagName("RECORDING") || child->getNumChildElements() != 0
            || scopeText.length() != 1 || !scopeText.containsOnly("01234")
            || slotText.length() != 1 || !slotText.containsOnly("01234567")) return false;
        auto& recording = state[static_cast<size_t>(scopeText.getIntValue())][static_cast<size_t>(slotText.getIntValue())];
        if (recording) return false;
        recording = decodeFrozenRecording(child->getStringAttribute("data"));
        if (!recording) return false;
    }
    return true;
}
inline void writeFrozenAudioState(juce::XmlElement& root, const FrozenRecordings& state)
{
    auto* media = root.createNewChildElement("FROZEN_AUDIO");
    media->setAttribute("version", 1);
    for (size_t scope = 0; scope < state.size(); ++scope)
        for (size_t slot = 0; slot < state[scope].size(); ++slot)
            if (const auto& recording = state[scope][slot]; recording && recording->isValid())
            {
                auto* child = media->createNewChildElement("RECORDING");
                child->setAttribute("scope", static_cast<int>(scope));
                child->setAttribute("slot", static_cast<int>(slot));
                child->setAttribute("data", encodeFrozenRecording(*recording));
            }
    if (media->getNumChildElements() == 0) root.removeChildElement(media, true);
}
}

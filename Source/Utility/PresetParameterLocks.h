#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <map>
#include <vector>

namespace fire::preset_locks
{
struct Entry { juce::String id, name; };
using Values = std::map<juce::String,float>;
inline constexpr int schemaVersion = 1;
inline constexpr const char* versionAttribute = "presetLocksVersion";
inline constexpr const char* valuesAttribute = "presetLockedParameters";
bool isLockableParameter(const juce::AudioProcessor&, const juce::String&);
std::vector<Entry> enumerate(const juce::AudioProcessor&);
Values captureValues(const juce::AudioProcessor&, const juce::StringArray&);
void writePreferences(juce::XmlElement&, const juce::StringArray&);
juce::StringArray readPreferences(const juce::XmlElement*, const juce::AudioProcessor&);
}

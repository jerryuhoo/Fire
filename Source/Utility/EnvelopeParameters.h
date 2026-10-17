#pragma once
#include <juce_core/juce_core.h>
#include <array>

namespace fire::envelope_params
{
inline constexpr int count = 4, schemaVersion = 1, parameterCount = 29;
enum Field { source, highPassEnabled, highPassFrequency, lowPassEnabled,
             lowPassFrequency, attack, release, sensitivity, fieldCount };
enum Source { mainInput, externalInput, band1, band2, band3, band4 };
struct Configuration
{
    int source = mainInput;
    bool highPassEnabled = false, lowPassEnabled = false;
    float highPassFrequency = 80, lowPassFrequency = 12000;
    float attack = 10, release = 200, sensitivity = 0;
};
using Bank = std::array<Configuration, count>;
inline constexpr std::array<const char*, fieldCount> suffixes{
    "Source", "HighPassOn", "HighPass", "LowPassOn", "LowPass", "Attack", "Release", "Sensitivity"};
inline constexpr std::array<const char*, fieldCount> labels{
    "Detector Source", "High Pass Enabled", "High Pass", "Low Pass Enabled", "Low Pass", "Attack", "Release", "Sensitivity"};
inline constexpr std::array<float, fieldCount> defaults{0,0,80,0,12000,10,200,0};
inline constexpr std::array<float, fieldCount> minimums{0,0,20,0,20,.1f,5,-24};
inline constexpr std::array<float, fieldCount> maximums{5,1,20000,1,20000,500,2000,24};
inline bool isAppended(int index, int field) noexcept { return index > 0 || field < attack; }
inline juce::String parameterID(int index, int field)
{
    if (index == 0 && field >= attack)
        return juce::String("env") + suffixes[static_cast<size_t>(field)];
    return "env" + juce::String(index + 1) + suffixes[static_cast<size_t>(field)];
}
inline const juce::StringArray& parameterIDs()
{
    static const auto ids = [] { juce::StringArray result;
        for (int env = 0; env < count; ++env) for (int field = 0; field < fieldCount; ++field)
            if (isAppended(env,field)) result.add(parameterID(env,field));
        return result; }();
    return ids;
}
inline bool isParameterID(const juce::String& id) { return parameterIDs().contains(id); }
inline const juce::StringArray& sourceNames()
{
    static const juce::StringArray names{"Main input", "External sidechain", "Band 1", "Band 2", "Band 3", "Band 4"};
    return names;
}
}

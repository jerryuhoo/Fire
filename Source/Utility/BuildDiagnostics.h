#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../GUI/InterfaceDefines.h"

#ifndef FIRE_BUILD_REVISION
 #define FIRE_BUILD_REVISION "local"
#endif
#ifndef FIRE_BUILD_CONFIGURATION
 #if JUCE_DEBUG
  #define FIRE_BUILD_CONFIGURATION "Debug"
 #else
  #define FIRE_BUILD_CONFIGURATION "Release"
 #endif
#endif

namespace fire::diagnostics
{
inline juce::String architecture()
{
#if defined(__aarch64__) || defined(_M_ARM64)
    return "arm64";
#elif defined(__x86_64__) || defined(_M_X64)
    return "x86_64";
#elif defined(__i386__) || defined(_M_IX86)
    return "x86";
#else
    return "unknown";
#endif
}

inline juce::String pluginFormat(const juce::AudioProcessor& processor)
{
    // The CLAP extension uses JUCE's undefined wrapper type. Resolve its
    // module name locally, without including an install path in copied reports.
    auto module = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    for (int depth = 0; depth < 4; ++depth, module = module.getParentDirectory())
        if (module.hasFileExtension("clap"))
            return "CLAP";
    if (processor.wrapperType == juce::AudioProcessor::wrapperType_Undefined)
        return "Internal / test host";
    return juce::AudioProcessor::getWrapperTypeDescription(processor.wrapperType);
}

juce::String buildIdentifier();

// Invoke on the message thread. This report contains technical configuration
// only: no preset contents, usernames, paths, device identifiers or telemetry.
inline juce::String report(const juce::AudioProcessor& processor, bool hq)
{
    juce::String result;
    result << "Fire " << VERSION << "\nBuild: " << buildIdentifier()
           << "\nFormat: " << pluginFormat(processor)
           << "\nArchitecture: " << architecture()
           << "\nOS: " << juce::SystemStats::getOperatingSystemName()
           << "\nJUCE: " << juce::SystemStats::getJUCEVersion()
           << "\nHost: " << juce::PluginHostType().getHostDescription()
           << "\nSample rate: " << juce::String(processor.getSampleRate(), 0) << " Hz"
           << "\nBlock size: " << processor.getBlockSize() << " samples"
           << "\nLatency: " << processor.getLatencySamples() << " samples"
           << "\nChannels: " << processor.getTotalNumInputChannels() << " in / "
           << processor.getTotalNumOutputChannels() << " out"
           << "\nHQ: " << (hq ? "On (4x Shape oversampling)" : "Off") << "\n";
    return result;
}
}

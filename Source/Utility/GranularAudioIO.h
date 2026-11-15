#pragma once
#include "../DSP/Clouds/FrozenRecording.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>

namespace fire::granular_io
{
inline constexpr double recordingSampleRate=32000.0;
inline constexpr double maximumDuration=effects::frozenRecordingFrames/recordingSampleRate;
struct FileStamp
{
    juce::String path;
    juce::int64 sizeBytes=0,modifiedMilliseconds=0,modifiedSeconds=0,modifiedTicks=0;
    bool exists=false;
    bool operator==(const FileStamp&) const=default;
};
struct AudioInfo
{
    double sampleRate=0;
    int channels=0;
    juce::int64 sourceFrames=0;
    double durationSeconds=0;
    FileStamp stamp;
};
struct ImportOptions {juce::int64 startFrame=0;bool normalise=false;};
struct ImportResult
{
    effects::FrozenRecordingPtr recording;
    juce::int64 sourceStartFrame=0;
    int outputFrames=0;
    double selectedSeconds=0,sourcePeak=0; // Peak after SRC, before optional normalisation.
    int clippedSamples=0,nonFiniteSamples=0; // Non-finite input count includes FIR guards.
    bool normalised=false;
};

// Worker-only file operations. Failures/cancellation leave output arguments and
// destination files untouched. No processor or live recording ring is accessed.
juce::String supportedFilePatterns();
juce::Result getFileStamp(const juce::File&,FileStamp&);
juce::Result inspectFile(const juce::File&,AudioInfo&);
juce::Result readWindow(const juce::File&,const AudioInfo& expected,ImportOptions,
                        ImportResult&,const std::atomic<bool>* cancelled=nullptr);
juce::File resolveWavePath(const juce::File&);
juce::Result exportWaveAtomic(const juce::File& finalPath,const effects::FrozenRecording&,
                             bool overwriteConfirmed,const std::atomic<bool>* cancelled=nullptr,
                             const FileStamp* expectedDestination=nullptr);
}

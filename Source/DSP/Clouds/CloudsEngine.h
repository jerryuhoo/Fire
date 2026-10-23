#pragma once

#include <memory>
#include <atomic>
#include "FrozenRecording.h"

namespace fire::effects
{
namespace frozen_audio
{
// Worker-thread-only conversion. Input includes any available source guards;
// firstSourceFrame is relative to that input, and may have a fractional part.
// Input must be prescaled to [-1,1]; output has no startup/guard padding.
int sourceGuardFrames(double sourceRate) noexcept;
bool resampleWindow(const float* left, const float* right, int inputFrames, double sourceRate,
                    double firstSourceFrame, float* outputLeft, float* outputRight, int outputFrames,
                    const std::atomic<bool>* cancelled = nullptr);
}

struct CloudsParameters
{
    float position = 0.1f, size = 0.5f, pitch = 0.0f, density = 0.25f;
    float texture = 0.5f, spread = 0.5f, feedback = 0.0f, reverb = 0.0f;
    bool freeze = false;
    std::uint32_t publicationSequence = 0;
};

// Normal stereo Clouds mode, behind a fixed 32 kHz / 32-frame core. The
// host owns dry/wet mixing; this returns wet audio including SRC/block latency.
class CloudsEngine
{
public:
    CloudsEngine() noexcept;
    ~CloudsEngine();
    CloudsEngine(const CloudsEngine&) = delete;
    CloudsEngine& operator=(const CloudsEngine&) = delete;

    void prepare(double sampleRate);
    void reset(bool preserveFrozen = false) noexcept;
    void process(float& left, float& right, const CloudsParameters&) noexcept;
    FrozenRecordingPtr copyFrozenRecording() const;
    // smoothReplacement is for a target-only material import. Authoritative
    // state restores keep their existing immediate publication semantics.
    void stageFrozenRecording(const FrozenRecordingPtr&, std::uint32_t publicationSequence = 0,
                              bool smoothReplacement = false);

private:
    struct Impl;
    struct RecordingExchange;
    std::unique_ptr<RecordingExchange> recordings;
    std::unique_ptr<Impl> implementation;
    void ensureRecordingExchange();
};
}

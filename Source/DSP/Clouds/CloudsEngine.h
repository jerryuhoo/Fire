#pragma once

#include <memory>

namespace fire::effects
{
struct CloudsParameters
{
    float position = 0.1f, size = 0.5f, pitch = 0.0f, density = 0.25f;
    float texture = 0.5f, spread = 0.5f, feedback = 0.0f, reverb = 0.0f;
    bool freeze = false;
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
    void reset() noexcept;
    void process(float& left, float& right, const CloudsParameters&) noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> implementation;
};
}

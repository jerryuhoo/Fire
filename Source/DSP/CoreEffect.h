#pragma once

#include "EqProcessor.h"
#include <juce_dsp/juce_dsp.h>
#include <memory>

namespace fire::effects
{
enum class Type;

// The insertable core modules use the same prepared engines as the original
// band modules. Construction and preparation stay outside the audio callback.
class CoreEffect
{
public:
    struct EqControl
    {
        float value = 0, depth = 0;
        int source = -1;
        bool bipolar = true;
        const float* signal = nullptr;
    };
    struct EqNode
    {
        eq::NodeState state;
        std::array<EqControl, 3> controls;
        std::uint32_t generation = 0;
    };
    CoreEffect();
    ~CoreEffect();
    void prepare(const juce::dsp::ProcessSpec&);
    void reset() noexcept;
    void process(juce::dsp::AudioBlock<float>, Type,
                 const std::array<ModulatedValueProvider, 6>&, bool normalised,
                 int offset, const std::array<int, 6>*,
                 const std::array<EqNode, eq::maxNodes>&) noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}

#pragma once
#include "../Utility/ModuleOrder.h"

namespace fire::effects
{
struct SlotParameters
{
    InsertEffect::Parameters effect;
    std::array<int, controlCount> sources {-1, -1, -1, -1, -1, -1};
    std::array<int, 3> cloudsSources {-1, -1, -1};
    int order = 0;
    SlotParameters() { effect.normalised = true; }
};
using RackParameters = std::array<SlotParameters, slotCount>;

class InsertRack
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        for (auto& effect : effects) effect.prepare(spec);
        dry.setSize(2, juce::jmax(1, static_cast<int>(spec.maximumBlockSize)));
        transition.reset(spec.sampleRate, 0.015);
        reset();
    }
    void reset() noexcept
    {
        for (auto& effect : effects) effect.reset();
        for (size_t i = 0; i < order.size(); ++i) order[i] = static_cast<int>(i);
        transition.setCurrentAndTargetValue(1.0f);
        initialised = false;
    }
    void process(juce::dsp::AudioBlock<float> block, const RackParameters& parameters,
                 const juce::AudioBuffer<float>& lfo, int sampleOffset = 0) noexcept
    {
        if (block.getNumSamples() > static_cast<size_t>(dry.getNumSamples()))
        {
            const auto capacity = static_cast<size_t>(dry.getNumSamples());
            if (capacity == 0) return;
            for (size_t start = 0; start < block.getNumSamples(); start += capacity)
                process(block.getSubBlock(start, juce::jmin(capacity, block.getNumSamples() - start)),
                        parameters, lfo, sampleOffset + static_cast<int>(start));
            return;
        }
        std::array<int, slotCount> requested;
        for (size_t i = 0; i < requested.size(); ++i) requested[i] = static_cast<int>(i);
        std::sort(requested.begin(), requested.end(), [&](int a, int b) {
            return parameters[static_cast<size_t>(a)].order == parameters[static_cast<size_t>(b)].order
                ? a < b : parameters[static_cast<size_t>(a)].order < parameters[static_cast<size_t>(b)].order;
        });
        if (! initialised) { order = requested; initialised = true; }
        if (order != requested && transition.getCurrentValue() == 0.0f) order = requested;
        transition.setTargetValue(order == requested ? 1.0f : 0.0f);
        const bool blending = transition.isSmoothing() || transition.getCurrentValue() < 1.0f;
        const auto channels = juce::jmin(size_t(2), block.getNumChannels());
        if (blending)
            for (size_t channel = 0; channel < channels; ++channel)
                juce::FloatVectorOperations::copy(dry.getWritePointer(static_cast<int>(channel)),
                    block.getChannelPointer(channel), static_cast<int>(block.getNumSamples()));
        for (int slot : order) processSlot(block, slot, parameters, lfo, sampleOffset);
        if (blending)
            for (size_t sample = 0; sample < block.getNumSamples(); ++sample)
            {
                const auto mix = transition.getNextValue();
                for (size_t channel = 0; channel < channels; ++channel)
                {
                    auto& wet = block.getChannelPointer(channel)[sample];
                    wet = juce::jmap(mix, dry.getSample(static_cast<int>(channel), static_cast<int>(sample)), wet);
                }
            }
    }
    void processSlot(juce::dsp::AudioBlock<float> block, int slot, const RackParameters& parameters,
                     const juce::AudioBuffer<float>& lfo, int sampleOffset = 0) noexcept
    {
        if (! juce::isPositiveAndBelow(slot, slotCount)) return;
        auto p = parameters[static_cast<size_t>(slot)].effect;
        for (size_t control = 0; control < controlCount; ++control)
        {
            const auto source = parameters[static_cast<size_t>(slot)].sources[control];
            p.values[control].lfoSignal = juce::isPositiveAndBelow(source, lfo.getNumChannels())
                && sampleOffset + static_cast<int>(block.getNumSamples()) <= lfo.getNumSamples()
                ? lfo.getReadPointer(source) : nullptr;
        }
        for (size_t control = 0; control < p.clouds.values.size(); ++control)
        {
            const auto source = parameters[static_cast<size_t>(slot)].cloudsSources[control];
            p.clouds.values[control].lfoSignal = juce::isPositiveAndBelow(source, lfo.getNumChannels())
                && sampleOffset >= 0 && sampleOffset + static_cast<int>(block.getNumSamples()) <= lfo.getNumSamples()
                ? lfo.getReadPointer(source) : nullptr;
        }
        effects[static_cast<size_t>(slot)].process(block, p, sampleOffset,
                                                  &parameters[static_cast<size_t>(slot)].sources,
                                                  &parameters[static_cast<size_t>(slot)].cloudsSources);
    }
private:
    std::array<InsertEffect, slotCount> effects;
    std::array<int, slotCount> order;
    juce::AudioBuffer<float> dry;
    juce::SmoothedValue<float> transition;
    bool initialised = false;
};
}

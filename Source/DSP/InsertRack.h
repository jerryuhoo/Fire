#pragma once
#include "../Utility/ModuleOrder.h"
#include "ShapeLatency.h"

namespace fire::effects
{
struct SlotParameters
{
    InsertEffect::Parameters effect;
    std::array<int, controlCount> sources {-1, -1, -1, -1, -1, -1};
    std::array<int, 3> cloudsSources {-1, -1, -1};
    int order = 0;
    int jitterSource = -1;
    int analogDriveSource = -1;
    SlotParameters() { effect.normalised = true; }
};
using RackParameters = std::array<SlotParameters, slotCount>;

class InsertRack
{
public:
    FrozenRecordingPtr copyFrozenRecording(int slot) const
    { return effects && juce::isPositiveAndBelow(slot, slotCount) ? (*effects)[static_cast<size_t>(slot)].copyFrozenRecording() : FrozenRecordingPtr{}; }
    void stageFrozenRecording(int slot, const FrozenRecordingPtr& recording, std::uint32_t publication)
    {
        if (! juce::isPositiveAndBelow(slot, slotCount)) return;
        if (! effects) effects = std::make_unique<std::array<InsertEffect, slotCount>>();
        (*effects)[static_cast<size_t>(slot)].stageFrozenRecording(recording, publication);
    }
    void prepare(const juce::dsp::ProcessSpec& spec, bool independentHq = false)
    {
        if (! effects) effects = std::make_unique<std::array<InsertEffect, slotCount>>();
        for (auto& effect : *effects) effect.prepare(spec);
        dry.setSize(2, juce::jmax(1, static_cast<int>(spec.maximumBlockSize)));
        supportsIndependentHq = independentHq;
        slotLatency = independentHq ? independentShapeLatency(spec.numChannels) : 0;
        if (independentHq)
        {
            slotInput.setSize(2, juce::jmax(1, static_cast<int>(spec.maximumBlockSize)));
            slotDry.setSize(2, juce::jmax(1, static_cast<int>(spec.maximumBlockSize)));
            orderDryDelay.setMaximumDelayInSamples(getReservedLatency());
            orderDryDelay.prepare(spec);
            orderDryDelay.setDelay(static_cast<float>(getReservedLatency()));
            for (auto* bank : {&slotRawDelay, &slotOutputDelay}) for (auto& delay : *bank)
            {
                delay.setMaximumDelayInSamples(slotLatency);
                delay.prepare(spec);
                delay.setDelay(static_cast<float>(slotLatency));
            }
        }
        transition.reset(spec.sampleRate, 0.015);
        reset();
    }
    void reset() noexcept
    {
        if (effects) for (auto& effect : *effects) effect.reset();
        for (auto* bank : {&slotRawDelay, &slotOutputDelay}) for (auto& delay : *bank) delay.reset();
        orderDryDelay.reset();
        for (size_t i = 0; i < order.size(); ++i) order[i] = static_cast<int>(i);
        transition.setCurrentAndTargetValue(1.0f);
        initialised = false;
    }
    int getReservedLatency() const noexcept {return slotLatency * slotCount;}
    void process(juce::dsp::AudioBlock<float> block, const RackParameters& parameters,
                 const juce::AudioBuffer<float>& lfo, int sampleOffset = 0, bool highQuality = false) noexcept
    {
        if (block.getNumSamples() > static_cast<size_t>(dry.getNumSamples()))
        {
            const auto capacity = static_cast<size_t>(dry.getNumSamples());
            if (capacity == 0) return;
            for (size_t start = 0; start < block.getNumSamples(); start += capacity)
                process(block.getSubBlock(start, juce::jmin(capacity, block.getNumSamples() - start)),
                        parameters, lfo, sampleOffset + static_cast<int>(start), highQuality);
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
        if (blending || supportsIndependentHq)
            for (size_t channel = 0; channel < channels; ++channel)
                juce::FloatVectorOperations::copy(dry.getWritePointer(static_cast<int>(channel)),
                    block.getChannelPointer(channel), static_cast<int>(block.getNumSamples()));
        if (supportsIndependentHq)
        {
            auto alignedDry = juce::dsp::AudioBlock<float>(dry).getSubsetChannelBlock(0, channels).getSubBlock(0, block.getNumSamples());
            orderDryDelay.process(juce::dsp::ProcessContextReplacing<float>(alignedDry));
        }
        for (int slot : order) processSlot(block, slot, parameters, lfo, sampleOffset, highQuality);
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
                     const juce::AudioBuffer<float>& lfo, int sampleOffset = 0, bool highQuality = false) noexcept
    {
        if (! effects || ! juce::isPositiveAndBelow(slot, slotCount)) return;
        auto p = parameters[static_cast<size_t>(slot)].effect;
        p.highQuality = supportsIndependentHq && highQuality;
        p.fixedShapeLatency = supportsIndependentHq;
        auto input = juce::dsp::AudioBlock<float>(slotInput);
        auto dryView = juce::dsp::AudioBlock<float>(slotDry);
        if (supportsIndependentHq)
        {
            input = input.getSubsetChannelBlock(0, block.getNumChannels()).getSubBlock(0, block.getNumSamples());
            dryView = dryView.getSubsetChannelBlock(0, block.getNumChannels()).getSubBlock(0, block.getNumSamples());
            input.copyFrom(block);
            slotRawDelay[static_cast<size_t>(slot)].process(juce::dsp::ProcessContextNonReplacing<float>(input, dryView));
            p.alignedShapeDry = &slotDry;
        }
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
        for (auto& node : p.eq)
            for (auto& control : node.controls)
                control.signal = juce::isPositiveAndBelow(control.source, lfo.getNumChannels())
                    && sampleOffset >= 0 && sampleOffset + static_cast<int>(block.getNumSamples()) <= lfo.getNumSamples()
                    ? lfo.getReadPointer(control.source) : nullptr;
        p.jitter.lfoSignal = juce::isPositiveAndBelow(parameters[static_cast<size_t>(slot)].jitterSource, lfo.getNumChannels())
            && sampleOffset >= 0 && sampleOffset + static_cast<int>(block.getNumSamples()) <= lfo.getNumSamples()
            ? lfo.getReadPointer(parameters[static_cast<size_t>(slot)].jitterSource) : nullptr;
        p.analogDrive.lfoSignal = juce::isPositiveAndBelow(parameters[static_cast<size_t>(slot)].analogDriveSource, lfo.getNumChannels())
            && sampleOffset >= 0 && sampleOffset + static_cast<int>(block.getNumSamples()) <= lfo.getNumSamples()
            ? lfo.getReadPointer(parameters[static_cast<size_t>(slot)].analogDriveSource) : nullptr;
        (*effects)[static_cast<size_t>(slot)].process(block, p, sampleOffset,
                                                  &parameters[static_cast<size_t>(slot)].sources,
                                                  &parameters[static_cast<size_t>(slot)].cloudsSources);
        if (supportsIndependentHq)
        {
            if ((*effects)[static_cast<size_t>(slot)].getProcessingLatency() != 0)
                slotOutputDelay[static_cast<size_t>(slot)].process(juce::dsp::ProcessContextNonReplacing<float>(input, dryView));
            else slotOutputDelay[static_cast<size_t>(slot)].process(juce::dsp::ProcessContextReplacing<float>(block));
        }
    }
private:
    std::unique_ptr<std::array<InsertEffect, slotCount>> effects;
    std::array<int, slotCount> order;
    juce::AudioBuffer<float> dry;
    juce::AudioBuffer<float> slotInput, slotDry;
    std::array<juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None>, slotCount> slotRawDelay, slotOutputDelay;
    int slotLatency = 0;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> orderDryDelay;
    bool supportsIndependentHq = false;
    juce::SmoothedValue<float> transition;
    bool initialised = false;
};
}

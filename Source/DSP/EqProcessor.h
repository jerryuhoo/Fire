#pragma once

#include "EqCoefficients.h"
#include "ModulatedValueProvider.h"

namespace fire::eq
{
// Additional/retyped EQ nodes. The three unchanged legacy nodes retain their
// original processor path so saved automation keeps its exact old response.
class Processor
{
public:
    struct Parameters
    {
        NodeState state;
        bool enabled = false;
        std::uint32_t generation = 0;
        std::array<ModulatedValueProvider, 3> controls; // frequency, gain, Q
        std::array<int, 3> sources { -1, -1, -1 };
    };

    void prepare(double rate) noexcept
    {
        sampleRate = std::isfinite(rate) && rate > 0.0 ? rate : 48000.0;
        // A fixed time grid, independent of host buffer boundaries. Coefficients
        // interpolate between updates, while parameter/LFO state advances on
        // every sample. Static nodes do no transcendental work in the loop.
        updatePeriod = juce::jmax(1, juce::roundToInt(sampleRate / 6000.0));
        for (auto& node : nodes)
        {
            for (auto& base : node.bases) base.reset(sampleRate, 0.02);
            for (auto& route : node.routes) route.blend.reset(sampleRate, 0.01);
            node.wet.reset(sampleRate, 0.02);
            node.typeBlend.reset(sampleRate, 0.02);
        }
        reset();
    }

    void reset() noexcept
    {
        for (auto& node : nodes)
        {
            node.initialised = false;
            node.enabled = false;
            node.wet.setCurrentAndTargetValue(0.0f);
            node.typeBlend.setCurrentAndTargetValue(0.0f);
            node.counter = 0;
            for (auto& bank : node.banks) bank.reset();
            for (auto& route : node.routes)
            {
                route.initialised = false;
                route.blend.setCurrentAndTargetValue(1.0f);
            }
        }
    }

    void begin(int slot, const Parameters& parameters) noexcept
    {
        if (! validSlot(slot)) return;
        auto& node = nodes[static_cast<size_t>(slot)];
        const bool wasDormant = ! node.initialised
                               || (! node.enabled && node.wet.getCurrentValue() == 0.0f);
        node.parameters = parameters;
        node.enabled = parameters.enabled;
        for (size_t control = 0; control < node.bases.size(); ++control)
        {
            const auto base = sanitise(control, parameters.controls[control].baseValue);
            if (wasDormant) node.bases[control].setCurrentAndTargetValue(base);
            else node.bases[control].setTargetValue(base);
            auto& route = node.routes[control];
            const auto& provider = parameters.controls[control];
            const bool routed = provider.lfoSignal != nullptr;
            const auto depth = std::isfinite(provider.modulationDepth)
                                   ? juce::jlimit(-1.0f, 1.0f, provider.modulationDepth) : 0.0f;
            if (! wasDormant && route.initialised
                && (route.routed != routed
                    || (routed && (route.source != parameters.sources[control]
                                   || ! juce::exactlyEqual(route.depth, depth)
                                   || route.bipolar != provider.isBipolar))))
            {
                route.anchor = node.values[control];
                route.blend.setCurrentAndTargetValue(0.0f);
                route.blend.setTargetValue(1.0f);
            }
            if (wasDormant)
            {
                route.blend.setCurrentAndTargetValue(1.0f);
                node.values[control] = base;
            }
            route.initialised = true;
            route.routed = routed;
            route.source = parameters.sources[control];
            route.depth = depth;
            route.bipolar = provider.isBipolar;
        }
        if (wasDormant && node.enabled)
        {
            node.banks[0].setRecipe(parameters.state, node.values, sampleRate, parameters.generation);
            node.banks[1].reset();
            node.typeBlend.setCurrentAndTargetValue(0.0f);
            node.counter = 0;
        }
        if (! node.initialised)
            node.wet.setCurrentAndTargetValue(node.enabled ? 1.0f : 0.0f);
        else node.wet.setTargetValue(node.enabled ? 1.0f : 0.0f);
        node.initialised = true;
        if (node.enabled) requestRecipe(node);
    }

    void process(int slot, juce::dsp::AudioBlock<float>& block, int start, int count) noexcept
    {
        if (! validSlot(slot) || count <= 0) return;
        auto& node = nodes[static_cast<size_t>(slot)];
        if (! node.initialised || (! node.enabled && node.wet.getCurrentValue() == 0.0f)) return;
        const auto channels = juce::jmin(size_t { 2 }, block.getNumChannels());
        for (int sample = start; sample < start + count; ++sample)
        {
            for (size_t control = 0; control < node.values.size(); ++control)
            {
                const auto base = node.bases[control].getNextValue();
                const auto target = sanitise(control, node.parameters.controls[control].get(sample, base));
                auto& route = node.routes[control];
                const auto blend = route.blend.getCurrentValue();
                node.values[control] = blend >= 1.0f ? target
                    : route.anchor + blend * (target - route.anchor);
                route.blend.getNextValue();
            }
            const int active = node.typeBlend.getCurrentValue() >= 0.5f ? 1 : 0;
            if (node.counter == 0)
            {
                node.banks[static_cast<size_t>(active)].update(node.values, sampleRate, updatePeriod);
                if (node.typeBlend.isSmoothing())
                    node.banks[static_cast<size_t>(1 - active)].update(node.values, sampleRate, updatePeriod);
                node.counter = updatePeriod;
            }
            --node.counter;
            const bool changingType = node.typeBlend.isSmoothing();
            node.banks[static_cast<size_t>(active)].advance();
            if (changingType) node.banks[static_cast<size_t>(1 - active)].advance();
            const auto typeMix = node.typeBlend.getCurrentValue();
            const auto wetMix = node.wet.getCurrentValue();
            for (size_t channel = 0; channel < channels; ++channel)
            {
                auto& output = block.getChannelPointer(channel)[sample];
                const auto dry = std::isfinite(output) ? output : 0.0f;
                auto wet = node.banks[static_cast<size_t>(active)].process(channel, dry);
                if (changingType)
                {
                    const auto other = node.banks[static_cast<size_t>(1 - active)].process(channel, dry);
                    wet = active == 0 ? wet + typeMix * (other - wet)
                                      : other + typeMix * (wet - other);
                }
                output = wetMix <= 0.0f ? dry : wetMix >= 1.0f ? wet : dry + wetMix * (wet - dry);
            }
            node.wet.getNextValue();
            node.typeBlend.getNextValue();
            if (changingType && ! node.typeBlend.isSmoothing() && node.enabled) requestRecipe(node);
        }
    }

    NodeState currentState(int slot) const noexcept
    {
        if (! validSlot(slot)) return {};
        const auto& node = nodes[static_cast<size_t>(slot)];
        auto state = node.parameters.state;
        state.frequency = node.values[0];
        state.gainDb = node.values[1];
        state.q = node.values[2];
        return state;
    }

private:
    struct Bank
    {
        NodeState recipe;
        std::uint32_t generation = 0;
        Coefficients coefficients, target;
        std::array<std::array<double, 6>, 5> increments {};
        std::array<std::array<std::array<double, 2>, 5>, 2> memory {};
        std::array<float, 3> previousValues {};
        int remaining = 0;

        void reset() noexcept { memory = {}; remaining = 0; coefficients = {}; target = {}; }
        void setRecipe(NodeState state, const std::array<float, 3>& values, double rate,
                       std::uint32_t newGeneration) noexcept
        {
            reset(); recipe = state; recipe.present = true; recipe.bypassed = false;
            generation = newGeneration;
            recipe.slope = isCut(recipe.type) ? juce::jlimit(0, 3, recipe.slope) : 0;
            setValues(values);
            coefficients = target = makeCoefficients(recipe, rate);
            previousValues = values;
        }
        void setValues(const std::array<float, 3>& values) noexcept
        { recipe.frequency = values[0]; recipe.gainDb = values[1]; recipe.q = values[2]; }
        void update(const std::array<float, 3>& values, double rate, int period) noexcept
        {
            if (values == previousValues) return;
            previousValues = values;
            setValues(values);
            target = makeCoefficients(recipe, rate);
            for (int stage = 0; stage < coefficients.numStages; ++stage)
                for (size_t coefficient = 0; coefficient < 6; ++coefficient)
                    increments[static_cast<size_t>(stage)][coefficient] =
                        (target.stages[static_cast<size_t>(stage)][coefficient]
                         - coefficients.stages[static_cast<size_t>(stage)][coefficient]) / period;
            remaining = period;
        }
        void advance() noexcept
        {
            if (remaining <= 0) return;
            if (--remaining == 0) { coefficients = target; return; }
            for (int stage = 0; stage < coefficients.numStages; ++stage)
                for (size_t coefficient = 0; coefficient < 6; ++coefficient)
                    coefficients.stages[static_cast<size_t>(stage)][coefficient]
                        += increments[static_cast<size_t>(stage)][coefficient];
        }
        float process(size_t channel, float input) noexcept
        {
            double value = input;
            for (int stage = 0; stage < coefficients.numStages; ++stage)
            {
                const auto index = static_cast<size_t>(stage);
                const auto& c = coefficients.stages[index];
                auto& z = memory[channel][index];
                const auto output = c[0] * value + z[0];
                z[0] = c[1] * value - c[4] * output + z[1];
                z[1] = c[2] * value - c[5] * output;
                if (! std::isfinite(output) || ! std::isfinite(z[0]) || ! std::isfinite(z[1]))
                { z = {}; value = 0.0; }
                else value = output;
            }
            const auto result = static_cast<float>(value);
            return std::isfinite(result) ? result : 0.0f;
        }
    };
    struct Route
    {
        juce::SmoothedValue<float> blend;
        float anchor = 0.0f, depth = 0.0f;
        int source = -1;
        bool routed = false, bipolar = true, initialised = false;
    };
    struct Node
    {
        Parameters parameters;
        std::array<Bank, 2> banks;
        std::array<juce::SmoothedValue<float>, 3> bases;
        std::array<Route, 3> routes;
        std::array<float, 3> values {};
        juce::SmoothedValue<float> wet, typeBlend;
        int counter = 0;
        bool initialised = false, enabled = false;
    };
    void requestRecipe(Node& node) noexcept
    {
        const auto requestedType = node.parameters.state.type;
        const auto requestedSlope = isCut(requestedType) ? node.parameters.state.slope : 0;
        const auto matches = [&](int bank)
        {
            const auto& recipe = node.banks[static_cast<size_t>(bank)].recipe;
            return recipe.type == requestedType && recipe.slope == requestedSlope
                   && node.banks[static_cast<size_t>(bank)].generation == node.parameters.generation;
        };
        if (node.typeBlend.isSmoothing())
        {
            const int target = node.typeBlend.getTargetValue() >= 0.5f ? 1 : 0;
            if (matches(1 - target)) node.typeBlend.setTargetValue(target == 1 ? 0.0f : 1.0f);
            return;
        }
        const int active = node.typeBlend.getCurrentValue() >= 0.5f ? 1 : 0;
        if (matches(active)) return;
        node.banks[static_cast<size_t>(1 - active)].setRecipe(node.parameters.state, node.values, sampleRate,
                                                           node.parameters.generation);
        node.typeBlend.setTargetValue(active == 0 ? 1.0f : 0.0f);
    }
    float sanitise(size_t control, float value) const noexcept
    {
        if (! std::isfinite(value)) return control == 0 ? 1000.0f : control == 1 ? 0.0f : 0.70710678f;
        if (control == 0)
        {
            const auto maximum = std::nextafter(static_cast<float>(sampleRate * 0.5), 0.0f);
            return juce::jlimit(juce::jmin(20.0f, maximum), maximum, value);
        }
        return control == 1 ? juce::jlimit(-24.0f, 24.0f, value) : juce::jlimit(0.1f, 18.0f, value);
    }
    std::array<Node, maxNodes> nodes;
    double sampleRate = 48000.0;
    int updatePeriod = 8;
};
} // namespace fire::eq

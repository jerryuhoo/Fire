#include "CoreEffect.h"
#include "../PluginProcessor.h"

namespace fire::effects
{
struct CoreEffect::Impl
{
    BandProcessor band;
    eq::Processor equalizer;
    juce::AudioBuffer<float> noLfo;
};
CoreEffect::CoreEffect() : impl(std::make_unique<Impl>()) {}
CoreEffect::~CoreEffect() = default;
void CoreEffect::prepare(const juce::dsp::ProcessSpec& spec)
{
    impl->band.prepare(spec, false);
    impl->equalizer.prepare(spec.sampleRate);
}
void CoreEffect::reset() noexcept
{
    impl->band.reset();
    impl->equalizer.reset();
}
void CoreEffect::process(juce::dsp::AudioBlock<float> block, Type type,
    const std::array<ModulatedValueProvider, 6>& values, bool normalised, int offset,
    const std::array<int, 6>* sources, const std::array<EqNode, eq::maxNodes>& eqNodes, int shapeModel,
    const ModulatedValueProvider& analogDrive, int analogDriveSource) noexcept
{
    if (block.getNumChannels() == 0 || block.getNumSamples() == 0) return;
    if (type == Type::eq)
    {
        for (int node = 0; node < eq::maxNodes; ++node)
        {
            const auto& input = eqNodes[static_cast<size_t>(node)];
            eq::Processor::Parameters p;
            p.state = input.state;
            p.enabled = input.state.present && ! input.state.bypassed;
            p.generation = input.generation;
            for (size_t i = 0; i < p.controls.size(); ++i)
            {
                const auto& c = input.controls[i];
                auto& provider = p.controls[i];
                provider.baseValue = c.value;
                provider.modulationDepth = c.depth;
                provider.isBipolar = c.bipolar;
                provider.lfoSignal = c.signal ? c.signal + offset : nullptr;
                provider.range = i == 0 ? juce::NormalisableRange<float>{20, 20000, 0, 0.3f}
                    : i == 1 ? juce::NormalisableRange<float>{-24, 24}
                             : juce::NormalisableRange<float>{0.1f, 20, 0, 0.35f};
                p.sources[i] = c.source;
            }
            impl->equalizer.begin(node, p);
            impl->equalizer.process(node, block, 0, static_cast<int>(block.getNumSamples()));
        }
        return;
    }
    BandProcessingParameters p;
    const auto provider = [&](size_t index, float multiplier = 1.0f)
    {
        auto result = values[index];
        const auto& definition = controls(type)[index];
        result.baseValue = (normalised ? definition.fromNormalised(result.baseValue) : result.baseValue) * multiplier;
        result.range = {definition.minimum * multiplier, definition.maximum * multiplier, 0, definition.skew};
        if (result.lfoSignal) result.lfoSignal += offset;
        return result;
    };
    const auto source = [&](size_t index) { return sources ? (*sources)[index] : -1; };
    // The enclosing insert owns the module Mix and power bridges.
    p.shapeMixVal = p.compMixVal = p.widthMixVal = 1.0f;
    p.shapeMixValProvider.baseValue = p.compMixValProvider.baseValue = p.widthMixValProvider.baseValue = 1.0f;
    p.mode = 3;
    if (type == Type::drive)
    {
        p.isDriveEnabled = true;
        p.driveVal = provider(0); p.driveLfoSourceIndex = source(0);
        p.isSafeModeOn = provider(1).baseValue >= 0.5f;
        p.isExtremeModeOn = provider(2).baseValue >= 0.5f;
        p.isOutputLinked = provider(3).baseValue >= 0.5f;
        p.useModernDriveComp = true;
    }
    else if (type == Type::shape)
    {
        p.isShapeEnabled = true;
        p.mode = fire::analog::resolve(juce::roundToInt(provider(0).baseValue), shapeModel);
        p.biasVal = provider(1); p.biasLfoSourceIndex = source(1);
        p.recVal = provider(2); p.recLfoSourceIndex = source(2);
        p.isDcFilterEnabled = provider(3).baseValue >= 0.5f;
        if (shapeModel > 0)
        {
            p.isDriveEnabled = true;
            p.isSafeModeOn = false;
            p.driveVal = analogDrive;
            p.driveLfoSourceIndex = analogDriveSource;
            if (p.driveVal.lfoSignal) p.driveVal.lfoSignal += offset;
        }
    }
    else if (type == Type::compressor)
    {
        p.isCompEnabled = true;
        p.compThresholdValProvider = provider(0); p.compThreshold = p.compThresholdValProvider.baseValue;
        p.compRatioValProvider = provider(1); p.compRatio = p.compRatioValProvider.baseValue;
        p.compAttackValProvider = provider(2); p.compAttack = p.compAttackValProvider.baseValue;
        p.compReleaseValProvider = provider(3); p.compRelease = p.compReleaseValProvider.baseValue;
        p.compThresholdLfoSourceIndex = source(0); p.compRatioLfoSourceIndex = source(1);
        p.compAttackLfoSourceIndex = source(2); p.compReleaseLfoSourceIndex = source(3);
    }
    else if (type == Type::stereo)
    {
        p.isWidthEnabled = true;
        p.widthValProvider = provider(0, 0.01f); p.width = p.widthValProvider.baseValue;
        p.panValProvider = provider(1, 0.01f); p.pan = p.panValProvider.baseValue;
        p.widthLfoSourceIndex = source(0); p.panLfoSourceIndex = source(1);
    }
    float* channels[2] {block.getChannelPointer(0), block.getChannelPointer(juce::jmin(size_t{1}, block.getNumChannels() - 1))};
    juce::AudioBuffer<float> buffer(channels, static_cast<int>(juce::jmin(size_t{2}, block.getNumChannels())), static_cast<int>(block.getNumSamples()));
    if (type == Type::drive || type == Type::shape)
    {
        impl->band.processDriveShapeStage(buffer, p, impl->noLfo, 0, buffer.getMagnitude(0, buffer.getNumSamples()), false,
                                         type == Type::drive || (type == Type::shape && shapeModel > 0), type == Type::shape);
        if (type == Type::shape) impl->band.processDcFilter(buffer, p.isDcFilterEnabled);
    }
    else if (type == Type::compressor) impl->band.processCompressorStage(buffer, p);
    else if (type == Type::stereo) impl->band.processStereoStage(buffer, p);
    else if (type == Type::ott)
    {
        p.ott.enabled = true;
        for (size_t i = 0; i < 5; ++i) {p.ott.controls[i] = provider(i, i == 0 ? 0.01f : 1.0f); p.ott.sources[i] = source(i);}
        p.ott.controls[5].baseValue = 1.0f;
        impl->band.ott.process(block, p.ott);
    }
}
}

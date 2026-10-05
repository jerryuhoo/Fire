#pragma once

#include "ModulatedValueProvider.h"
#include "Clouds/CloudsEngine.h"
#include "ChordResonator.h"
#include "CoreEffect.h"
#include "SpatialReverb.h"
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <limits>
#include <vector>
#include <atomic>
#include <mutex>

namespace fire::effects
{
enum class Type { none = 0, chorus = 1, delay = 2, reverb = 3, granular = 4, lofi = 5,
                  flanger = 6, phaser = 7, chordResonator = 8,
                  drive = 9, shape = 10, compressor = 11, ott = 12, stereo = 13, eq = 14, count = 15 };
inline bool isCore(Type type) noexcept { return type >= Type::drive && type <= Type::eq; }
inline constexpr size_t controlCount = 6;

struct Control
{
    const char* name;
    const char* unit;
    float minimum, maximum, initial, skew = 1.0f;
    juce::NormalisableRange<float> range() const { return {minimum, maximum, 0.0f, skew}; }
    float fromNormalised(float value) const noexcept
    {
        value = juce::jlimit(0.0f, 1.0f, std::isfinite(value) ? value : 0.0f);
        return minimum + (maximum - minimum) * (skew == 1.0f ? value : std::pow(value, 1.0f / skew));
    }
    float toNormalised(float value) const noexcept
    {
        const auto proportion = juce::jlimit(0.0f, 1.0f, (value - minimum) / (maximum - minimum));
        return skew == 1.0f ? proportion : std::pow(proportion, skew);
    }
};

inline const char* name(Type type) noexcept
{
    switch (type)
    {
        case Type::chorus: return "Chorus";
        case Type::delay: return "Delay";
        case Type::reverb: return "Reverb";
        case Type::granular: return "Granular";
        case Type::lofi: return "Lo-Fi";
        case Type::flanger: return "Flanger";
        case Type::phaser: return "Phaser";
        case Type::chordResonator: return "Chord Resonator";
        case Type::drive: return "Drive";
        case Type::shape: return "Shape";
        case Type::compressor: return "Compressor";
        case Type::ott: return "OTT";
        case Type::stereo: return "Stereo";
        case Type::eq: return "EQ";
        case Type::none: case Type::count: return "Empty";
    }
    return "Empty";
}

inline const std::array<Control, controlCount>& controls(Type type)
{
    static const std::array<Control, controlCount> chorus {{
        {"Rate", " Hz", 0.05f, 8, 0.8f, 0.4f}, {"Depth", " %", 0, 100, 45},
        {"Delay", " ms", 5, 30, 16}, {"Feedback", " %", -75, 75, 10},
        {"Width", " %", 0, 100, 85}, {"Mix", " %", 0, 100, 35}
    }};
    static const std::array<Control, controlCount> delay {{
        {"Time", " ms", 10, 2000, 375, 0.5f}, {"Feedback", " %", 0, 90, 35},
        {"Tone", " Hz", 500, 18000, 7000, 0.4f}, {"Ping-Pong", " %", 0, 100, 70},
        {"Sync", "", 0, 7, 0}, {"Mix", " %", 0, 100, 25}
    }};
    static const std::array<Control, controlCount> reverb {{
        {"Size", " %", 0, 100, 65}, {"Damping", " %", 0, 100, 40},
        {"Pre-delay", " ms", 0, 200, 18}, {"Width", " %", 0, 100, 100},
        {"Low Cut", " Hz", 20, 1500, 120, 0.45f}, {"Mix", " %", 0, 100, 25}
    }};
    static const std::array<Control, controlCount> granular {{
        {"Size", " %", 0, 100, 50}, {"Density", " %", -100, 100, -50},
        {"Pitch", " st", -24, 24, 0}, {"Position", " %", 0, 100, 10},
        {"Texture", " %", 0, 100, 50}, {"Mix", " %", 0, 100, 35}
    }};
    static const std::array<Control, controlCount> lofi {{
        {"Rate", " x", 1, 32, 1, 0.5f}, {"Bits", " bit", 4, 24, 16},
        {"Tape", " %", 0, 100, 30}, {"Wow", " %", 0, 100, 10},
        {"Flutter", " %", 0, 100, 10}, {"Mix", " %", 0, 100, 50}
    }};
    static const std::array<Control, controlCount> flanger {{
        {"Rate", " Hz", 0.05f, 8, 0.25f, 0.4f}, {"Depth", " %", 0, 100, 65},
        {"Delay", " ms", 0.1f, 10, 2}, {"Feedback", " %", -90, 90, 40},
        {"Width", " %", 0, 100, 75}, {"Mix", " %", 0, 100, 50}
    }};
    static const std::array<Control, controlCount> phaser {{
        {"Rate", " Hz", 0.05f, 8, 0.35f, 0.4f}, {"Depth", " %", 0, 100, 70},
        {"Center", " Hz", 40, 6000, 900, 0.36f}, {"Feedback", " %", -85, 85, 30},
        {"Width", " %", 0, 100, 75}, {"Mix", " %", 0, 100, 50}
    }};
    static const std::array<Control, controlCount> chordResonator {{
        {"Root", "", 36, 72, 48}, {"Chord", "", 0, 7, 3},
        {"Color", " %", 0, 100, 55}, {"Decay", " s", 0.05f, 3, 0.6f, 0.42f},
        {"Width", " %", 0, 100, 70}, {"Mix", " %", 0, 100, 35}
    }};
    static const std::array<Control, controlCount> drive {{
        {"Drive", "", 0, 100, 0}, {"Safe", "", 0, 1, 1}, {"Extreme", "", 0, 1, 0},
        {"Comp", "", 0, 1, 1}, {"", "", 0, 1, 0}, {"Mix", " %", 0, 100, 100}
    }};
    static const std::array<Control, controlCount> shape {{
        {"Mode", "", 0, 11, 3}, {"Bias", "", -1, 1, 0}, {"Rectify", "", 0, 1, 0},
        {"DC Filter", "", 0, 1, 0}, {"", "", 0, 1, 0}, {"Mix", " %", 0, 100, 100}
    }};
    static const std::array<Control, controlCount> compressor {{
        {"Threshold", " dB", -60, 0, 0}, {"Ratio", " :1", 1, 20, 1},
        {"Attack", " ms", 0.1f, 200, 10, 0.35f}, {"Release", " ms", 1, 1000, 100, 0.35f},
        {"", "", 0, 1, 0}, {"Mix", " %", 0, 100, 100}
    }};
    static const std::array<Control, controlCount> ott {{
        {"Depth", " %", 0, 100, 50}, {"Time", " %", 10, 400, 100},
        {"Upward", " dB", -72, -6, -48}, {"Downward", " dB", -36, 0, -18},
        {"Output", " dB", -24, 24, 0}, {"Mix", " %", 0, 100, 100}
    }};
    static const std::array<Control, controlCount> stereo {{
        {"Width", " %", 0, 100, 50}, {"Pan", " %", -100, 100, 0}, {"", "", 0, 1, 0},
        {"", "", 0, 1, 0}, {"", "", 0, 1, 0}, {"Mix", " %", 0, 100, 100}
    }};
    static const std::array<Control, controlCount> eq {{
        {"", "", 0, 1, 0}, {"", "", 0, 1, 0}, {"", "", 0, 1, 0},
        {"", "", 0, 1, 0}, {"", "", 0, 1, 0}, {"Mix", " %", 0, 100, 100}
    }};
    switch (type)
    {
        case Type::delay: return delay;
        case Type::reverb: return reverb;
        case Type::granular: return granular;
        case Type::lofi: return lofi;
        case Type::flanger: return flanger;
        case Type::phaser: return phaser;
        case Type::chordResonator: return chordResonator;
        case Type::drive: return drive;
        case Type::shape: return shape;
        case Type::compressor: return compressor;
        case Type::ott: return ott;
        case Type::stereo: return stereo;
        case Type::eq: return eq;
        case Type::chorus: case Type::none: case Type::count: return chorus;
    }
    return chorus;
}

inline float sanitise(Type type, size_t index, float value) noexcept
{
    const auto& definition = controls(type)[index];
    return std::isfinite(value) ? juce::jlimit(definition.minimum, definition.maximum, value) : definition.initial;
}

// Keep the expanded Granular control view on the same canonical descriptors.
inline const std::array<Control, controlCount>& cloudsControls()
{
    return controls(Type::granular);
}

// A bounded fractional history with a logical reset. Old slots cannot leak
// into a newly inserted effect, and clearing a long delay never blocks audio.
class StereoHistory
{
public:
    void prepare(double sampleRate, double seconds)
    {
        const auto size = static_cast<size_t>(std::ceil(sampleRate * seconds)) + 8;
        for (auto& channel : data) channel.assign(size, 0.0f);
        reset();
    }
    void reset() noexcept { head = filled = 0; }
    void write(float left, float right) noexcept
    {
        if (data[0].empty()) return;
        data[0][head] = std::isfinite(left) ? left : 0.0f;
        data[1][head] = std::isfinite(right) ? right : 0.0f;
        if (++head == data[0].size()) head = 0;
        filled = juce::jmin(filled + 1, data[0].size());
    }
    float read(size_t channel, float age) const noexcept
    {
        if (data[0].empty() || ! std::isfinite(age) || age < 1.0f
            || static_cast<double>(age) >= static_cast<double>(data[0].size() - 1)) return 0.0f;
        const auto whole = static_cast<size_t>(age);
        if (whole > filled) return 0.0f;
        const auto index = head >= whole ? head - whole : data[0].size() - (whole - head);
        const auto previous = index == 0 ? data[0].size() - 1 : index - 1;
        // Before the first recorded sample the logical history is silence.
        // Requiring both interpolation taps to be filled drops the first
        // integer echo, and the leading part of a fractional echo, after reset.
        const auto previousValue = whole < filled ? data[channel][previous] : 0.0f;
        return juce::jmap(age - static_cast<float>(whole), data[channel][index], previousValue);
    }

private:
    std::array<std::vector<float>, 2> data;
    size_t head = 0, filled = 0;
};

// Shared by the existing Master Lo-Fi and insertable Lo-Fi instances.
class TapeFlutter
{
public:
    void prepare(double rate)
    {
        sampleRate = rate;
        toneCoefficient = static_cast<float>(1.0 - std::exp(-juce::MathConstants<double>::twoPi
            * juce::jmin(11000.0, sampleRate * 0.4) / sampleRate));
        history.prepare(rate, 0.05);
        reset();
    }
    void reset() noexcept { history.reset(); tone.fill(0); phase = 0; }
    void process(float& left, float& right, float tape, float wow, float flutter) noexcept
    {
        tape = juce::jlimit(0.0f, 1.0f, std::isfinite(tape) ? tape : 0.0f);
        wow = juce::jlimit(0.0f, 1.0f, std::isfinite(wow) ? wow : 0.0f);
        flutter = juce::jlimit(0.0f, 1.0f, std::isfinite(flutter) ? flutter : 0.0f);
        history.write(left, right);
        phase += 1.0 / sampleRate;
        if (phase >= 1000.0) phase = std::fmod(phase, 1000.0);
        if (tape == 0.0f && wow == 0.0f && flutter == 0.0f) return;
        const auto pitchMix = juce::jlimit(0.0f, 1.0f, (wow + flutter) * 8.0f);
        const auto modulation = wow * 0.004 * std::sin(phase * 0.43 * juce::MathConstants<double>::twoPi)
            + flutter * 0.0007 * (std::sin(phase * 6.7 * juce::MathConstants<double>::twoPi)
                                  + 0.35 * std::sin(phase * 11.3 * juce::MathConstants<double>::twoPi));
        const auto delay = static_cast<float>((0.008 + modulation) * sampleRate);
        float* channels[] {&left, &right};
        for (size_t channel = 0; channel < 2; ++channel)
        {
            auto value = juce::jmap(pitchMix, *channels[channel], history.read(channel, delay));
            const auto drive = 1.0f + 5.0f * tape;
            const auto saturated = std::tanh(value * drive) / drive * (1.0f + tape * 0.65f);
            tone[channel] += toneCoefficient * (saturated - tone[channel]);
            *channels[channel] = juce::jmap(tape, value, tone[channel]);
        }
    }
private:
    StereoHistory history;
    std::array<float, 2> tone {};
    float toneCoefficient = 0.0f;
    double sampleRate = 48000, phase = 0;
};

class InsertEffect
{
public:
    struct Parameters
    {
        struct CloudsControls
        {
            bool freeze = false;
            // Spread, feedback, reverb have their own appended host IDs.
            std::array<ModulatedValueProvider, 3> values;
            CloudsControls()
            {
                for (auto& value : values) value.range = {0.0f, 1.0f};
                values[0].baseValue = 0.5f;
            }
        } clouds;
        Type type = Type::none;
        std::uint32_t publicationSequence = 0;
        bool enabled = true;
        bool normalised = false;
        bool highQuality = false, fixedShapeLatency = false;
        const juce::AudioBuffer<float>* alignedShapeDry = nullptr;
        float bpm = 120;
        std::array<ModulatedValueProvider, controlCount> values;
        std::array<CoreEffect::EqNode, eq::maxNodes> eq;
        ModulatedValueProvider jitter;
        int shapeModel = 0;
        int reverbModel = 0;
        ModulatedValueProvider analogDrive;
        int analogDriveSource = -1;
        explicit Parameters(Type kind = Type::none) : type(kind)
        {
            for (size_t i = 0; i < values.size(); ++i)
            {
                values[i].baseValue = controls(kind)[i].initial;
                values[i].range = controls(kind)[i].range();
            }
        }
    };

    void prepare(const juce::dsp::ProcessSpec& spec, bool eager = true)
    {
        const std::lock_guard<std::mutex> lock(preparationLock);
        const auto previousFamilies = preparedFamilies.exchange(0, std::memory_order_acq_rel);
        preparedSpec = spec;
        preparationInitialised = true;
        sampleRate = std::isfinite(spec.sampleRate) && spec.sampleRate > 0 ? spec.sampleRate : 48000.0;
        prepareFamiliesUnlocked(eager ? allFamilies : previousFamilies);
        gate.reset(sampleRate, 0.02);
        for (auto& smoother : bases) smoother.reset(sampleRate, 0.02);
        for (auto& route : routes) route.blend.reset(sampleRate, 0.01);
        for (auto& smoother : cloudsBases) smoother.reset(sampleRate, 0.02);
        for (auto& route : cloudsRoutes) route.blend.reset(sampleRate, 0.01);
        flangerWet.reset(sampleRate, 0.02);
        flangerRecordGain.reset(sampleRate, 0.005);
        flangerWarmupLength = static_cast<int>(std::ceil(sampleRate * 0.02));
        phaserCoefficientPeriod = juce::jmax(1, juce::roundToInt(sampleRate / 6000.0));
        reset();
    }
    void reset(bool preserveFrozen = false) noexcept
    {
        preserveFrozenOnActivation = preserveFrozen && (preserveFrozenOnActivation || currentType == Type::granular);
        resetMemory(preserveFrozen);
        currentType = Type::none;
        gate.setCurrentAndTargetValue(0);
        dormant = true;
    }
    // These methods are lifecycle/state/worker APIs. Audio only requests a
    // missing family with a lock-free mask and retains the dry path until ready.
    bool prepareForType(Type type) noexcept {return prepareFamilies(familyFor(type));}
    void prepareRequestedFamilies() noexcept
    {
        const auto requested = requestedFamilies.exchange(0, std::memory_order_acq_rel);
        if (requested) prepareFamilies(requested);
    }
    bool isPreparedForType(Type type) const noexcept
    {
        const auto family = familyFor(type);
        return (preparedFamilies.load(std::memory_order_acquire) & family) == family;
    }
    FrozenRecordingPtr copyFrozenRecording() const
    {const std::lock_guard<std::mutex> lock(preparationLock); return cloudsEngine.copyFrozenRecording();}
    void stageFrozenRecording(const FrozenRecordingPtr& recording, std::uint32_t publication)
    {const std::lock_guard<std::mutex> lock(preparationLock); cloudsEngine.stageFrozenRecording(recording, publication);}
    int getProcessingLatency() const noexcept
    {return currentType == Type::shape && fixedShapeLatencyActive && core ? core->getShapeLatency() : 0;}
    void process(juce::dsp::AudioBlock<float> block, const Parameters& parameters, int offset = 0,
                 const std::array<int, controlCount>* sourceIndices = nullptr,
                 const std::array<int, 3>* cloudsSourceIndices = nullptr) noexcept
    {
        if (block.getNumChannels() == 0 || block.getNumSamples() == 0) return;
        fixedShapeLatencyActive = parameters.fixedShapeLatency || parameters.highQuality;
        auto requested = parameters.type;
        if (requested < Type::none || requested >= Type::count) requested = Type::none;
        if (!isPreparedForType(requested))
        {
            requestedFamilies.fetch_or(familyFor(requested), std::memory_order_release);
            requested = Type::none;
        }
        const auto matches = [&] { return requested == currentType && (currentType != Type::reverb
            || currentReverbModel == juce::jlimit(0, fire::space::count - 1, parameters.reverbModel)); };
        if (requested == Type::none && currentType == Type::none) return;
        if (currentType == Type::none && requested != Type::none) activate(requested, parameters);
        if (! matches() && juce::exactlyEqual(gate.getCurrentValue(), 0.0f)) activate(requested, parameters);
        if (matches()) updateControlTargets(parameters, sourceIndices, cloudsSourceIndices);
        gate.setTargetValue(parameters.enabled && matches() && currentType != Type::none ? 1.0f : 0.0f);
        if (isCore(currentType))
        {
            processCore(block, parameters, offset, sourceIndices, matches());
            return;
        }
        auto* left = block.getChannelPointer(0);
        auto* right = block.getNumChannels() > 1 ? block.getChannelPointer(1) : nullptr;
        for (size_t sample = 0; sample < block.getNumSamples(); ++sample)
        {
            if (gate.getCurrentValue() == 0.0f && ! matches() && ! isCore(requested))
            {
                activate(requested, parameters);
                updateControlTargets(parameters, sourceIndices, cloudsSourceIndices);
                gate.setTargetValue(parameters.enabled && currentType != Type::none ? 1.0f : 0.0f);
            }
            const auto enabled = gate.getNextValue();
            if (enabled == 0.0f)
            {
                if (! dormant) { resetMemory(); dormant = true; }
                continue;
            }
            dormant = false;
            std::array<float, controlCount> value;
            for (size_t i = 0; i < controlCount; ++i)
            {
                const auto smoothedBase = bases[i].getNextValue();
                if (! matches()) { value[i] = lastValues[i]; continue; }
                // Note/chord selections must not sweep through intermediate
                // menu items. Their engine owns a bounded retuning crossfade.
                const bool discrete = currentType == Type::chordResonator && i < 2;
                const auto base = discrete ? baseValue(parameters, i) : smoothedBase;
                const auto modulated = parameters.values[i].get(offset + static_cast<int>(sample), base);
                // Keep nonlinear control mappings sample accurate, while
                // avoiding pow() when an unmodulated value has settled.
                if (currentNormalised)
                {
                    value[i] = juce::exactlyEqual(modulated, lastNormalisedValues[i])
                        ? mappedValues[i] : currentControls()[i].fromNormalised(modulated);
                    lastNormalisedValues[i] = modulated;
                }
                else
                {
                    const auto& definition = currentControls()[i];
                    value[i] = std::isfinite(modulated)
                        ? juce::jlimit(definition.minimum, definition.maximum, modulated) : definition.initial;
                }
                // Cache the unbridged target separately: a constant routed
                // value must not reuse the previous step of its own fade.
                mappedValues[i] = value[i];
                auto& route = routes[i];
                const auto blend = route.blend.getCurrentValue();
                if (! discrete)
                {
                    if (blend <= 0.0f) value[i] = route.anchor;
                    else if (blend < 1.0f) value[i] = route.anchor + blend * (value[i] - route.anchor);
                }
                lastValues[i] = value[i];
                route.blend.getNextValue();
            }
            const auto dryL = std::isfinite(left[sample]) ? left[sample] : 0.0f;
            const auto dryR = right && std::isfinite(right[sample]) ? right[sample] : dryL;
            float wetL = dryL, wetR = dryR;
            if (currentType == Type::granular)
            {
                if (matches())
                {
                    cloudsState.freeze = parameters.clouds.freeze;
                    for (size_t i = 0; i < cloudsBases.size(); ++i)
                    {
                        auto next = parameters.clouds.values[i].get(offset + static_cast<int>(sample),
                                                                    cloudsBases[i].getNextValue());
                        next = juce::jlimit(0.0f, 1.0f, std::isfinite(next) ? next : 0.0f);
                        auto& route = cloudsRoutes[i];
                        const auto blend = route.blend.getCurrentValue();
                        lastCloudsValues[i] = juce::jmap(blend, route.anchor, next);
                        route.blend.getNextValue();
                    }
                }
                cloudsState.size = value[0] * 0.01f;
                cloudsState.density = (value[1] + 100.0f) * 0.005f;
                cloudsState.pitch = value[2];
                cloudsState.position = value[3] * 0.01f;
                cloudsState.texture = value[4] * 0.01f;
                cloudsState.spread = lastCloudsValues[0];
                cloudsState.feedback = lastCloudsValues[1];
                cloudsState.reverb = lastCloudsValues[2];
                cloudsState.publicationSequence = parameters.publicationSequence;
                cloudsEngine.process(wetL, wetR, cloudsState);
                // A stereo granular field folds down symmetrically in a
                // mono host; listening only to its left grains loses energy.
                if (! right) wetL = 0.5f * (wetL + wetR);
            }
            else
                processSample(wetL, wetR, value, parameters.bpm, right != nullptr,
                    matches() ? parameters.jitter.get(offset + static_cast<int>(sample)) : 0.0f);
            auto mix = enabled * value[5] * 0.01f;
            if (currentType == Type::flanger)
            {
                // Cold delay history is not an audible signal. Record under
                // dry audio for the maximum sweep length, then fade in wet.
                if (flangerWarmupRemaining > 0) --flangerWarmupRemaining;
                else flangerWet.setTargetValue(1.0f);
                mix *= flangerWet.getNextValue();
            }
            left[sample] = juce::jmap(mix, dryL, std::isfinite(wetL) ? wetL : 0.0f);
            if (right) right[sample] = juce::jmap(mix, dryR, std::isfinite(wetR) ? wetR : 0.0f);
        }
    }
private:
    enum : std::uint32_t {historyFamily = 1, tapeFamily = 2, reverbFamily = 4,
        cloudsFamily = 8, chordFamily = 16, coreFamily = 32, allFamilies = 63};
    static std::uint32_t familyFor(Type type) noexcept
    {
        if (isCore(type)) return coreFamily;
        if (type == Type::chorus || type == Type::delay || type == Type::flanger) return historyFamily;
        if (type == Type::lofi) return tapeFamily;
        if (type == Type::reverb) return historyFamily | reverbFamily;
        if (type == Type::granular) return cloudsFamily;
        if (type == Type::chordResonator) return chordFamily;
        return 0;
    }
    bool prepareFamilies(std::uint32_t requested) noexcept
    {
        if ((preparedFamilies.load(std::memory_order_acquire) & requested) == requested) return true;
        try
        {
            const std::lock_guard<std::mutex> lock(preparationLock);
            if (preparationInitialised) prepareFamiliesUnlocked(requested);
        }
        catch (...) {} // Retry off audio; no partially prepared family is published.
        const auto missing = requested & ~preparedFamilies.load(std::memory_order_acquire);
        if (missing) requestedFamilies.fetch_or(missing, std::memory_order_release);
        return missing == 0;
    }
    void prepareFamiliesUnlocked(std::uint32_t requested)
    {
        const auto missing = requested & ~preparedFamilies.load(std::memory_order_relaxed);
        for (const auto family : {historyFamily, tapeFamily, reverbFamily, cloudsFamily, chordFamily, coreFamily})
        {
            if (!(missing & family)) continue;
            if (family == historyFamily) history.prepare(sampleRate, 2.3);
            else if (family == tapeFamily) tape.prepare(sampleRate);
            else if (family == reverbFamily) {reverb.setSampleRate(sampleRate); spatialReverb.prepare(sampleRate);}
            else if (family == cloudsFamily) cloudsEngine.prepare(sampleRate);
            else if (family == chordFamily) chordEngine.prepare(sampleRate);
            else if (family == coreFamily)
            {
                if (!core) core = std::make_unique<CoreEffect>();
                core->prepare(preparedSpec);
                const auto capacity = juce::jmax(1, static_cast<int>(preparedSpec.maximumBlockSize));
                coreWet.setSize(2, capacity); shapeDry.setSize(2, capacity);
                shapeDryDelay.prepare(preparedSpec);
                shapeDryDelay.setDelay(static_cast<float>(core->getShapeLatency()));
            }
            preparedFamilies.fetch_or(family, std::memory_order_release);
        }
    }
    void processCore(juce::dsp::AudioBlock<float> block, const Parameters& requested,
                     int offset, const std::array<int, controlCount>* sources, bool matches) noexcept
    {
        if (! core || coreWet.getNumSamples() == 0) return;
        if (block.getNumSamples() > static_cast<size_t>(coreWet.getNumSamples()))
        {
            const auto capacity = static_cast<size_t>(coreWet.getNumSamples());
            for (size_t start = 0; start < block.getNumSamples(); start += capacity)
                processCore(block.getSubBlock(start, juce::jmin(capacity, block.getNumSamples() - start)),
                    requested, offset + static_cast<int>(start), sources, matches);
            return;
        }
        if (matches)
        {
            coreLastParameters = requested;
            for (auto& value : coreLastParameters.values) value.lfoSignal = nullptr;
            coreLastParameters.analogDrive.lfoSignal = nullptr;
            coreLastParameters.alignedShapeDry = nullptr;
            for (auto& node : coreLastParameters.eq) for (auto& value : node.controls) value.signal = nullptr;
        }
        const bool alignShape = currentType == Type::shape && fixedShapeLatencyActive;
        auto alignedDry = juce::dsp::AudioBlock<float>(shapeDry)
            .getSubsetChannelBlock(0, juce::jmin(size_t{2}, block.getNumChannels())).getSubBlock(0, block.getNumSamples());
        if (alignShape)
        {
            if (requested.alignedShapeDry)
                alignedDry.copyFrom(juce::dsp::AudioBlock<const float>(*requested.alignedShapeDry)
                    .getSubsetChannelBlock(0, alignedDry.getNumChannels()).getSubBlock(0, block.getNumSamples()));
            else shapeDryDelay.process(juce::dsp::ProcessContextNonReplacing<float>(block, alignedDry));
        }
        if (juce::exactlyEqual(gate.getCurrentValue(), 0.0f) && ! gate.isSmoothing())
        {
            if (! dormant) {core->reset(); dormant = true;}
            if (alignShape) block.copyFrom(alignedDry);
            return;
        }
        dormant = false;
        auto wet = juce::dsp::AudioBlock<float>(coreWet).getSubsetChannelBlock(0, juce::jmin(size_t{2}, block.getNumChannels()))
            .getSubBlock(0, block.getNumSamples());
        wet.copyFrom(block);
        const auto& p = matches ? requested : coreLastParameters;
        core->process(wet, currentType, p.values, currentNormalised, matches ? offset : 0, sources,
                      p.eq, p.shapeModel, p.analogDrive, p.analogDriveSource,
                      requested.highQuality, fixedShapeLatencyActive);
        for (size_t sample = 0; sample < block.getNumSamples(); ++sample)
        {
            const auto base = bases[5].getNextValue();
            auto mix = matches ? p.values[5].get(offset + static_cast<int>(sample), base) : lastValues[5];
            if (matches && currentNormalised) mix = controls(currentType)[5].fromNormalised(mix);
            mix = juce::jlimit(0.0f, 100.0f, std::isfinite(mix) ? mix : 0.0f);
            const auto blend = routes[5].blend.getNextValue();
            if (matches && blend < 1.0f) mix = juce::jmap(blend, routes[5].anchor, mix);
            lastValues[5] = mix;
            const auto amount = gate.getNextValue() * mix * 0.01f;
            for (size_t channel = 0; channel < wet.getNumChannels(); ++channel)
            {
                auto& original = block.getChannelPointer(channel)[sample];
                const auto result = wet.getChannelPointer(channel)[sample];
                const auto drySample = alignShape ? alignedDry.getChannelPointer(channel)[sample] : original;
                original = juce::jmap(amount, drySample, std::isfinite(result) ? result : 0.0f);
            }
        }
    }
    struct Route
    {
        juce::SmoothedValue<float> blend;
        float anchor = 0.0f, depth = 0.0f;
        int source = -1;
        bool initialised = false, routed = false, bipolar = true;
    };
    void updateControlTargets(const Parameters& parameters,
                              const std::array<int, controlCount>* sourceIndices,
                              const std::array<int, 3>* cloudsSourceIndices) noexcept
    {
        for (size_t i = 0; i < controlCount; ++i)
        {
            bases[i].setTargetValue(baseValue(parameters, i));
            const auto& provider = parameters.values[i];
            auto& route = routes[i];
            const bool routed = provider.lfoSignal != nullptr;
            // Source identity is independent of the buffer address, which
            // may change every host callback or internal processing chunk.
            const int source = ! routed ? -1
                : sourceIndices != nullptr ? (*sourceIndices)[i] : 0;
            const auto depth = std::isfinite(provider.modulationDepth)
                ? juce::jlimit(-1.0f, 1.0f, provider.modulationDepth) : 0.0f;
            if (route.initialised && (route.routed != routed
                || (routed && (route.source != source
                    || ! juce::exactlyEqual(route.depth, depth)
                    || route.bipolar != provider.isBipolar))))
            {
                route.anchor = lastValues[i];
                route.blend.setCurrentAndTargetValue(0.0f);
                route.blend.setTargetValue(1.0f);
            }
            route.initialised = true;
            route.routed = routed;
            route.source = source;
            route.depth = depth;
            route.bipolar = provider.isBipolar;
        }
        if (currentType == Type::granular)
            for (size_t i = 0; i < cloudsBases.size(); ++i)
            {
                const auto& provider = parameters.clouds.values[i];
                cloudsBases[i].setTargetValue(safeCloudsBase(provider));
                auto& route = cloudsRoutes[i];
                const bool routed = provider.lfoSignal != nullptr;
                const int source = ! routed ? -1 : cloudsSourceIndices != nullptr ? (*cloudsSourceIndices)[i] : 0;
                const auto depth = std::isfinite(provider.modulationDepth)
                    ? juce::jlimit(-1.0f, 1.0f, provider.modulationDepth) : 0.0f;
                if (route.initialised && (route.routed != routed || (routed && (route.source != source
                    || ! juce::exactlyEqual(route.depth, depth) || route.bipolar != provider.isBipolar))))
                {
                    route.anchor = lastCloudsValues[i];
                    route.blend.setCurrentAndTargetValue(0.0f);
                    route.blend.setTargetValue(1.0f);
                }
                route.initialised = true; route.routed = routed; route.source = source;
                route.depth = depth; route.bipolar = provider.isBipolar;
            }
    }
    void resetMemory(bool preserveFrozen = true) noexcept
    {
        const auto ready = preparedFamilies.load(std::memory_order_acquire);
        if (ready & historyFamily) history.reset();
        if (ready & tapeFamily) tape.reset();
        if (ready & reverbFamily) {reverb.reset(); spatialReverb.reset();}
        if (ready & cloudsFamily) cloudsEngine.reset(preserveFrozen);
        if (ready & chordFamily) chordEngine.reset();
        if (ready & coreFamily) {core->reset(); shapeDryDelay.reset();}
        feedback.fill(0); highPassInput.fill(0); highPassOutput.fill(0);
        held.fill(0); holdRemaining = 0; holdResidual = 0; jitterSeed = 0x61c88647u;
        phase = 0;
        flangerWarmupRemaining = flangerWarmupLength;
        flangerWet.setCurrentAndTargetValue(0.0f);
        flangerRecordGain.setCurrentAndTargetValue(0.0f);
        flangerRecordGain.setTargetValue(1.0f);
        lastNormalisedValues.fill(std::numeric_limits<float>::quiet_NaN());
        lastDelayTone = lastReverbLowCut = lastBits = -1.0f;
        reverbSettingsValid = false;
        for (auto& channel : phaserMemory) channel.fill(0.0);
        phaserFeedback.fill(0.0);
        phaserLastFrequency.fill(-1.0);
        phaserCoefficientCounter = phaserCoefficientRamp = 0;
        phaserCoefficientsReady = false;
        for (auto& route : routes)
        {
            route.initialised = false;
            route.blend.setCurrentAndTargetValue(1.0f);
        }
        for (auto& route : cloudsRoutes)
        {
            route.initialised = false;
            route.blend.setCurrentAndTargetValue(1.0f);
        }
    }
    void activate(Type type, const Parameters& parameters) noexcept
    {
        // A rack/lifecycle reset primes controls again, but must not turn the
        // same granular instance's retained recording into an empty capture.
        resetMemory(preserveFrozenOnActivation && type == Type::granular);
        preserveFrozenOnActivation = false;
        currentType = type; currentNormalised = parameters.normalised;
        currentReverbModel = juce::jlimit(0, fire::space::count - 1, parameters.reverbModel);
        cloudsState.freeze = parameters.clouds.freeze;
        for (size_t i = 0; i < controlCount; ++i)
        {
            bases[i].setCurrentAndTargetValue(baseValue(parameters, i));
            lastValues[i] = currentNormalised ? currentControls()[i].fromNormalised(baseValue(parameters, i))
                                               : baseValue(parameters, i);
            mappedValues[i] = lastValues[i];
        }
        for (size_t i = 0; i < cloudsBases.size(); ++i)
        {
            lastCloudsValues[i] = safeCloudsBase(parameters.clouds.values[i]);
            cloudsBases[i].setCurrentAndTargetValue(lastCloudsValues[i]);
        }
    }
    static float baseValue(const Parameters& parameters, size_t index) noexcept
    {
        const auto value = parameters.values[index].baseValue;
        if (parameters.normalised) return juce::jlimit(0.0f, 1.0f, std::isfinite(value) ? value : 0.0f);
        const auto& definition = controls(parameters.type)[index];
        return std::isfinite(value) ? juce::jlimit(definition.minimum, definition.maximum, value) : definition.initial;
    }
    const std::array<Control, controlCount>& currentControls() const noexcept
    { return controls(currentType); }
    static float safeCloudsBase(const ModulatedValueProvider& provider) noexcept
    { return juce::jlimit(0.0f, 1.0f, std::isfinite(provider.baseValue) ? provider.baseValue : 0.0f); }
    void updatePhaserCoefficients(const std::array<float, controlCount>& p, bool stereo) noexcept
    {
        const auto maxFrequency = sampleRate * 0.45;
        const auto minFrequency = juce::jmin(20.0, maxFrequency);
        const auto angle = phase * juce::MathConstants<double>::twoPi;
        std::array<double, 2> frequency;
        for (size_t channel = 0; channel < frequency.size(); ++channel)
        {
            if (channel == 1 && (! stereo || p[4] == 0.0f)) { frequency[1] = frequency[0]; continue; }
            const auto shift = channel == 1 ? static_cast<double>(p[4]) * 0.01 * juce::MathConstants<double>::pi : 0.0;
            const auto sweep = p[1] > 0.0f
                ? std::exp2(static_cast<double>(p[1]) * 0.025 * std::sin(angle + shift)) : 1.0;
            frequency[channel] = juce::jlimit(minFrequency, maxFrequency, static_cast<double>(p[2]) * sweep);
        }
        if (phaserCoefficientsReady && frequency == phaserLastFrequency) return;
        phaserLastFrequency = frequency;
        for (size_t channel = 0; channel < frequency.size(); ++channel)
        {
            if (channel == 1 && juce::exactlyEqual(frequency[1], frequency[0]))
            {
                phaserTargetA[1] = phaserTargetA[0];
                phaserTargetB[1] = phaserTargetB[0];
            }
            else
            {
                const auto tangent = std::tan(juce::MathConstants<double>::pi * frequency[channel] / sampleRate);
                phaserTargetA[channel] = (tangent - 1.0) / (tangent + 1.0);
                phaserTargetB[channel] = 2.0 * std::sqrt(tangent) / (tangent + 1.0);
            }
            if (! phaserCoefficientsReady)
            {
                phaserA[channel] = phaserTargetA[channel];
                phaserB[channel] = phaserTargetB[channel];
            }
            phaserStepA[channel] = (phaserTargetA[channel] - phaserA[channel]) / phaserCoefficientPeriod;
            phaserStepB[channel] = (phaserTargetB[channel] - phaserB[channel]) / phaserCoefficientPeriod;
        }
        phaserCoefficientRamp = phaserCoefficientsReady ? phaserCoefficientPeriod : 0;
        phaserCoefficientsReady = true;
    }
    void processPhaser(float& left, float& right, const std::array<float, controlCount>& p, bool stereo) noexcept
    {
        if (phaserCoefficientCounter == 0)
        {
            updatePhaserCoefficients(p, stereo);
            phaserCoefficientCounter = phaserCoefficientPeriod;
        }
        --phaserCoefficientCounter;
        if (phaserCoefficientRamp > 0)
        {
            if (--phaserCoefficientRamp == 0) { phaserA = phaserTargetA; phaserB = phaserTargetB; }
            else
                for (size_t channel = 0; channel < phaserA.size(); ++channel)
                { phaserA[channel] += phaserStepA[channel]; phaserB[channel] += phaserStepB[channel]; }
        }
        const auto amount = static_cast<double>(p[3]) * 0.01;
        const auto inputGain = 1.0 - std::abs(amount);
        float* channels[] {&left, &right};
        for (size_t channel = 0; channel < (stereo ? size_t {2} : size_t {1}); ++channel)
        {
            auto sample = static_cast<double>(*channels[channel]) * inputGain + amount * phaserFeedback[channel];
            const auto a = phaserA[channel], b = phaserB[channel];
            for (auto& memory : phaserMemory[channel])
            {
                // Normalised first-order lattice: H(z)=(a+z^-1)/(1+a*z^-1).
                // [a b; b -a] is orthogonal when a*a+b*b=1. Interpolating a/b
                // together follows the unit-circle chord (norm <= 1), so even
                // rapidly varying coefficients cannot inject state energy.
                const auto output = a * sample + b * memory;
                memory = b * sample - a * memory;
                sample = output;
            }
            if (! std::isfinite(sample))
            {
                phaserMemory[channel].fill(0.0);
                sample = 0.0;
            }
            phaserFeedback[channel] = sample;
            const auto output = static_cast<float>(sample);
            *channels[channel] = std::isfinite(output) ? output : 0.0f;
        }
    }
    void processSample(float& left, float& right, const std::array<float, controlCount>& p, float bpm, bool stereo, float jitter = 0.0f) noexcept
    {
        const auto sr = static_cast<float>(sampleRate);
        if (currentType == Type::chorus)
        {
            phase += p[0] / sampleRate;
            if (phase >= 1.0) phase = std::fmod(phase, 1.0);
            const auto center = p[2] * 0.001f * sr;
            const auto depth = juce::jmin(center * 0.8f, 0.009f * sr) * p[1] * 0.01f;
            const auto angle = static_cast<float>(phase) * juce::MathConstants<float>::twoPi;
            const auto l = history.read(0, center + depth * std::sin(angle));
            const auto r = history.read(1, center + depth * std::sin(angle + p[4] * 0.01f * juce::MathConstants<float>::pi));
            history.write(left + l * p[3] * 0.01f, right + r * p[3] * 0.01f);
            left = l; right = r;
        }
        else if (currentType == Type::flanger)
        {
            phase += p[0] / sampleRate;
            if (phase >= 1.0) phase -= std::floor(phase);
            const auto center = juce::jmax(1.0f, p[2] * 0.001f * sr);
            // The read always precedes this sample's write. Keeping the sweep
            // above one sample avoids an algebraic feedback loop at low rates.
            const auto excursion = (center - 1.0f) * 0.95f * p[1] * 0.01f;
            const auto angle = static_cast<float>(phase) * juce::MathConstants<float>::twoPi;
            const auto delayL = excursion > 0.0f ? center + excursion * std::sin(angle) : center;
            const auto delayR = stereo && excursion > 0.0f && p[4] > 0.0f
                ? center + excursion * std::sin(angle + p[4] * 0.01f * juce::MathConstants<float>::pi) : delayL;
            const auto l = history.read(0, delayL);
            const auto r = stereo ? history.read(1, delayR) : l;
            const auto amount = p[3] * 0.01f;
            // Fade only the start of a new recording. Otherwise feedback can
            // repeat its initial zero-to-signal edge after the wet fade starts.
            const auto inputGain = (1.0f - std::abs(amount)) * flangerRecordGain.getNextValue();
            // Convex fractional reads and |feedback| <= .9 bound the loop;
            // normalising its input also prevents extreme resonance gain.
            history.write(left * inputGain + l * amount,
                          (stereo ? right : left) * inputGain + r * amount);
            left = l; right = r;
        }
        else if (currentType == Type::phaser)
        {
            phase += p[0] / sampleRate;
            if (phase >= 1.0) phase -= std::floor(phase);
            processPhaser(left, right, p, stereo);
        }
        else if (currentType == Type::chordResonator)
        {
            fire::chord_resonator::Parameters parameters;
            parameters.root = p[0]; parameters.chord = p[1];
            parameters.color = p[2] * 0.01f; parameters.decay = p[3]; parameters.width = p[4] * 0.01f;
            chordEngine.process(left, right, parameters, stereo);
        }
        else if (currentType == Type::delay)
        {
            constexpr float beats[] {0, 0.25f, 0.5f, 0.75f, 1, 1.5f, 2, 4};
            const auto sync = juce::jlimit(0, 7, juce::roundToInt(p[4]));
            bpm = std::isfinite(bpm) && bpm > 0 ? bpm : 120;
            const auto seconds = sync == 0 ? p[0] * 0.001f : juce::jlimit(0.01f, 2.0f, beats[sync] * 60.0f / bpm);
            const auto l = history.read(0, seconds * sr);
            const auto r = history.read(1, seconds * sr);
            if (! juce::exactlyEqual(p[2], lastDelayTone))
            {
                lastDelayTone = p[2];
                delayToneCoefficient = 1.0f - std::exp(-juce::MathConstants<float>::twoPi * juce::jmin(p[2], sr * 0.4f) / sr);
            }
            feedback[0] += delayToneCoefficient * (l - feedback[0]);
            feedback[1] += delayToneCoefficient * (r - feedback[1]);
            const auto cross = p[3] * 0.01f;
            // At full ping-pong, launch the mono sum into one side before
            // alternating feedback. Cross-feedback alone leaves centred mono
            // input identical in both channels and never produces a bounce.
            const auto inputLeft = juce::jmap(cross, left, 0.5f * (left + right));
            const auto inputRight = right * (1.0f - cross);
            history.write(inputLeft + juce::jmap(cross, feedback[0], feedback[1]) * p[1] * 0.01f,
                          inputRight + juce::jmap(cross, feedback[1], feedback[0]) * p[1] * 0.01f);
            left = l; right = r;
        }
        else if (currentType == Type::reverb)
        {
            history.write(left, right);
            if (p[2] > 0.01f) { left = history.read(0, juce::jmax(1.0f, p[2] * sr * 0.001f)); right = history.read(1, juce::jmax(1.0f, p[2] * sr * 0.001f)); }
            if (! juce::exactlyEqual(p[4], lastReverbLowCut))
            {
                lastReverbLowCut = p[4];
                reverbLowCutCoefficient = std::exp(-juce::MathConstants<float>::twoPi * juce::jmin(p[4], sr * 0.4f) / sr);
            }
            float* channels[] {&left, &right};
            for (size_t c = 0; c < 2; ++c)
            {
                const auto value = reverbLowCutCoefficient * (highPassOutput[c] + *channels[c] - highPassInput[c]);
                highPassInput[c] = *channels[c]; highPassOutput[c] = value; *channels[c] = value;
            }
            if (currentReverbModel > 0)
            {
                spatialReverb.process(left, right, currentReverbModel, p[0], p[1], p[3]);
                return;
            }
            juce::Reverb::Parameters settings;
            settings.roomSize = p[0] * 0.01f; settings.damping = p[1] * 0.01f;
            settings.wetLevel = 1; settings.dryLevel = 0; settings.width = p[3] * 0.01f; settings.freezeMode = 0;
            const auto& previous = reverb.getParameters();
            if (! reverbSettingsValid
                || ! juce::exactlyEqual(settings.roomSize, previous.roomSize)
                || ! juce::exactlyEqual(settings.damping, previous.damping)
                || ! juce::exactlyEqual(settings.width, previous.width))
            {
                reverb.setParameters(settings);
                reverbSettingsValid = true;
            }
            reverb.processStereo(&left, &right, 1);
        }
        else if (currentType == Type::lofi)
        {
            if (holdRemaining-- <= 0)
            {
                jitter = juce::jlimit(0.0f, 1.0f, std::isfinite(jitter) ? jitter : 0.0f);
                jitterSeed ^= jitterSeed << 13; jitterSeed ^= jitterSeed >> 17; jitterSeed ^= jitterSeed << 5;
                const auto random = static_cast<float>(jitterSeed & 0xffffu) / 65535.0f;
                const auto period = juce::jmax(1.0f, p[0] * (1.0f + jitter * (random * 2.0f - 1.0f))) + holdResidual;
                const auto whole = std::floor(period);
                holdRemaining = juce::jmax(0, static_cast<int>(whole) - 1);
                holdResidual = period - whole;
                if (! juce::exactlyEqual(p[1], lastBits))
                {
                    lastBits = p[1];
                    quantisationSteps = std::pow(2.0f, p[1] - 1.0f);
                }
                held = {std::round(left * quantisationSteps) / quantisationSteps,
                        std::round(right * quantisationSteps) / quantisationSteps};
            }
            left = held[0]; right = held[1];
            tape.process(left, right, p[2] * 0.01f, p[3] * 0.01f, p[4] * 0.01f);
        }
    }
    double sampleRate = 48000, phase = 0;
    mutable std::mutex preparationLock;
    std::atomic<std::uint32_t> preparedFamilies{0}, requestedFamilies{0};
    juce::dsp::ProcessSpec preparedSpec{48000, 1, 2};
    bool preparationInitialised = false;
    Type currentType = Type::none;
    std::unique_ptr<CoreEffect> core;
    juce::AudioBuffer<float> coreWet;
    juce::AudioBuffer<float> shapeDry;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> shapeDryDelay{64};
    bool fixedShapeLatencyActive = false;
    Parameters coreLastParameters;
    std::uint32_t jitterSeed = 0x61c88647u;
    bool dormant = true, currentNormalised = false, preserveFrozenOnActivation = false;
    CloudsEngine cloudsEngine;
    fire::chord_resonator::Engine chordEngine;
    CloudsParameters cloudsState;
    std::array<juce::SmoothedValue<float>, 3> cloudsBases;
    std::array<Route, 3> cloudsRoutes;
    std::array<float, 3> lastCloudsValues {};
    StereoHistory history;
    TapeFlutter tape;
    juce::Reverb reverb;
    fire::space::Reverb spatialReverb;
    int currentReverbModel = 0;
    std::array<juce::SmoothedValue<float>, controlCount> bases;
    std::array<Route, controlCount> routes;
    std::array<float, controlCount> lastValues {};
    std::array<float, controlCount> mappedValues {};
    std::array<float, controlCount> lastNormalisedValues {};
    float lastDelayTone = -1.0f, delayToneCoefficient = 0.0f;
    float lastReverbLowCut = -1.0f, reverbLowCutCoefficient = 0.0f;
    float lastBits = -1.0f, quantisationSteps = 1.0f;
    bool reverbSettingsValid = false;
    juce::SmoothedValue<float> gate;
    juce::SmoothedValue<float> flangerWet, flangerRecordGain;
    int flangerWarmupLength = 960, flangerWarmupRemaining = 960;
    std::array<float, 2> feedback {}, highPassInput {}, highPassOutput {}, held {};
    int holdRemaining = 0;
    float holdResidual = 0;
    std::array<std::array<double, 6>, 2> phaserMemory {};
    std::array<double, 2> phaserFeedback {}, phaserA {}, phaserB {}, phaserTargetA {}, phaserTargetB {},
        phaserStepA {}, phaserStepB {}, phaserLastFrequency {};
    int phaserCoefficientPeriod = 8, phaserCoefficientCounter = 0, phaserCoefficientRamp = 0;
    bool phaserCoefficientsReady = false;
};
}

#pragma once

#include "ModulatedValueProvider.h"
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <limits>
#include <vector>

namespace fire::effects
{
enum class Type { none, chorus, delay, reverb, granular, lofi, count };
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
        {"Size", " ms", 10, 250, 90, 0.6f}, {"Density", " /s", 2, 40, 16},
        {"Pitch", " st", -24, 24, 0}, {"Position", " ms", 0, 1200, 120, 0.5f},
        {"Spray", " %", 0, 100, 30}, {"Mix", " %", 0, 100, 35}
    }};
    static const std::array<Control, controlCount> lofi {{
        {"Rate", " x", 1, 32, 1, 0.5f}, {"Bits", " bit", 4, 24, 16},
        {"Tape", " %", 0, 100, 30}, {"Wow", " %", 0, 100, 10},
        {"Flutter", " %", 0, 100, 10}, {"Mix", " %", 0, 100, 50}
    }};
    switch (type)
    {
        case Type::delay: return delay;
        case Type::reverb: return reverb;
        case Type::granular: return granular;
        case Type::lofi: return lofi;
        case Type::chorus: case Type::none: case Type::count: return chorus;
    }
    return chorus;
}

inline float sanitise(Type type, size_t index, float value) noexcept
{
    const auto& definition = controls(type)[index];
    return std::isfinite(value) ? juce::jlimit(definition.minimum, definition.maximum, value) : definition.initial;
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
        if (data[0].empty() || ! std::isfinite(age) || age < 1.0f) return 0.0f;
        const auto whole = static_cast<size_t>(age);
        if (whole + 1 > filled || whole + 1 >= data[0].size()) return 0.0f;
        const auto index = head >= whole ? head - whole : data[0].size() - (whole - head);
        const auto previous = index == 0 ? data[0].size() - 1 : index - 1;
        return juce::jmap(age - static_cast<float>(whole), data[channel][index], data[channel][previous]);
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
        Type type = Type::none;
        bool enabled = true;
        bool normalised = false;
        float bpm = 120;
        std::array<ModulatedValueProvider, controlCount> values;
        explicit Parameters(Type kind = Type::none) : type(kind)
        {
            for (size_t i = 0; i < values.size(); ++i)
            {
                values[i].baseValue = controls(kind)[i].initial;
                values[i].range = controls(kind)[i].range();
            }
        }
    };

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = std::isfinite(spec.sampleRate) && spec.sampleRate > 0 ? spec.sampleRate : 48000.0;
        history.prepare(sampleRate, 2.3);
        tape.prepare(sampleRate);
        reverb.setSampleRate(sampleRate);
        gate.reset(sampleRate, 0.02);
        for (auto& smoother : bases) smoother.reset(sampleRate, 0.02);
        reset();
    }
    void reset() noexcept
    {
        resetMemory();
        currentType = Type::none;
        gate.setCurrentAndTargetValue(0);
        dormant = true;
    }
    void process(juce::dsp::AudioBlock<float> block, const Parameters& parameters, int offset = 0) noexcept
    {
        if (block.getNumChannels() == 0 || block.getNumSamples() == 0) return;
        auto requested = parameters.type;
        if (requested < Type::none || requested >= Type::count) requested = Type::none;
        if (requested == Type::none && currentType == Type::none) return;
        if (currentType == Type::none && requested != Type::none) activate(requested, parameters);
        for (size_t i = 0; i < controlCount; ++i)
            if (requested == currentType) bases[i].setTargetValue(baseValue(parameters, i));
        gate.setTargetValue(parameters.enabled && requested == currentType && currentType != Type::none ? 1.0f : 0.0f);
        auto* left = block.getChannelPointer(0);
        auto* right = block.getNumChannels() > 1 ? block.getChannelPointer(1) : nullptr;
        for (size_t sample = 0; sample < block.getNumSamples(); ++sample)
        {
            if (gate.getCurrentValue() == 0.0f && requested != currentType)
            {
                activate(requested, parameters);
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
                const auto base = bases[i].getNextValue();
                if (requested != currentType) { value[i] = lastValues[i]; continue; }
                const auto modulated = parameters.values[i].get(offset + static_cast<int>(sample), base);
                // Keep nonlinear control mappings sample accurate, while
                // avoiding pow() when an unmodulated value has settled.
                if (currentNormalised)
                {
                    value[i] = juce::exactlyEqual(modulated, lastNormalisedValues[i])
                        ? lastValues[i] : controls(currentType)[i].fromNormalised(modulated);
                    lastNormalisedValues[i] = modulated;
                }
                else
                    value[i] = sanitise(currentType, i, modulated);
                lastValues[i] = value[i];
            }
            const auto dryL = std::isfinite(left[sample]) ? left[sample] : 0.0f;
            const auto dryR = right && std::isfinite(right[sample]) ? right[sample] : dryL;
            float wetL = dryL, wetR = dryR;
            processSample(wetL, wetR, value, parameters.bpm);
            const auto mix = enabled * value[5] * 0.01f;
            left[sample] = juce::jmap(mix, dryL, std::isfinite(wetL) ? wetL : 0.0f);
            if (right) right[sample] = juce::jmap(mix, dryR, std::isfinite(wetR) ? wetR : 0.0f);
        }
    }
private:
    struct Grain { float age = 0, length = 1, delay = 1, speed = 1, pan = 0; bool active = false; };
    void resetMemory() noexcept
    {
        history.reset(); tape.reset(); reverb.reset();
        feedback.fill(0); highPassInput.fill(0); highPassOutput.fill(0);
        held.fill(0); holdRemaining = 0; holdResidual = 0;
        grains.fill({}); untilGrain = 0; phase = 0; randomState = 0x5a17c9e3u;
        lastNormalisedValues.fill(std::numeric_limits<float>::quiet_NaN());
        lastDelayTone = lastReverbLowCut = lastBits = -1.0f;
        reverbSettingsValid = false;
    }
    void activate(Type type, const Parameters& parameters) noexcept
    {
        resetMemory(); currentType = type; currentNormalised = parameters.normalised;
        for (size_t i = 0; i < controlCount; ++i)
        {
            bases[i].setCurrentAndTargetValue(baseValue(parameters, i));
            lastValues[i] = currentNormalised ? controls(type)[i].fromNormalised(baseValue(parameters, i))
                                               : baseValue(parameters, i);
        }
    }
    static float baseValue(const Parameters& parameters, size_t index) noexcept
    {
        const auto value = parameters.values[index].baseValue;
        return parameters.normalised ? juce::jlimit(0.0f, 1.0f, std::isfinite(value) ? value : 0.0f)
                                     : sanitise(parameters.type, index, value);
    }
    float random() noexcept
    {
        randomState ^= randomState << 13; randomState ^= randomState >> 17; randomState ^= randomState << 5;
        return static_cast<float>(randomState & 0xffffffu) / 16777215.0f;
    }
    void processSample(float& left, float& right, const std::array<float, controlCount>& p, float bpm) noexcept
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
        else if (currentType == Type::granular)
        {
            history.write(left, right);
            if (--untilGrain <= 0)
            {
                untilGrain = juce::jmax(1, juce::roundToInt(sr / p[1]));
                for (auto& grain : grains)
                    if (! grain.active)
                    {
                        grain.active = true; grain.age = 0; grain.length = p[0] * sr * 0.001f;
                        grain.speed = std::pow(2.0f, p[2] / 12.0f);
                        grain.delay = (p[3] + (random() - 0.5f) * p[4] * 4.0f) * sr * 0.001f;
                        grain.delay = juce::jlimit(1.0f + grain.length * juce::jmax(0.0f, grain.speed - 1.0f), sr * 2.0f, grain.delay);
                        grain.pan = (random() - 0.5f) * p[4] * 0.01f;
                        break;
                    }
            }
            left = right = 0;
            float weight = 0;
            for (auto& grain : grains)
                if (grain.active)
                {
                    const auto window = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * grain.age / grain.length);
                    const auto age = grain.delay + grain.age * (1.0f - grain.speed);
                    left += history.read(0, age) * window * (1.0f - grain.pan);
                    right += history.read(1, age) * window * (1.0f + grain.pan);
                    weight += window;
                    if (++grain.age >= grain.length) grain.active = false;
                }
            const auto normalisation = 1.0f / juce::jmax(1.0f, weight);
            left *= normalisation; right *= normalisation;
        }
        else if (currentType == Type::lofi)
        {
            if (holdRemaining-- <= 0)
            {
                const auto period = p[0] + holdResidual;
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
    Type currentType = Type::none;
    bool dormant = true, currentNormalised = false;
    StereoHistory history;
    TapeFlutter tape;
    juce::Reverb reverb;
    std::array<juce::SmoothedValue<float>, controlCount> bases;
    std::array<float, controlCount> lastValues {};
    std::array<float, controlCount> lastNormalisedValues {};
    float lastDelayTone = -1.0f, delayToneCoefficient = 0.0f;
    float lastReverbLowCut = -1.0f, reverbLowCutCoefficient = 0.0f;
    float lastBits = -1.0f, quantisationSteps = 1.0f;
    bool reverbSettingsValid = false;
    juce::SmoothedValue<float> gate;
    std::array<float, 2> feedback {}, highPassInput {}, highPassOutput {}, held {};
    std::array<Grain, 12> grains;
    int untilGrain = 0, holdRemaining = 0;
    float holdResidual = 0;
    std::uint32_t randomState = 0x5a17c9e3u;
};
}

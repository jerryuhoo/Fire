#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <algorithm>
#include <cmath>

namespace fire::chord_resonator
{
inline constexpr int minimumRoot = 36, maximumRoot = 72, defaultRoot = 48, defaultChord = 3;
inline constexpr int harmonicCount = 6, maximumNotes = 4, maximumModes = harmonicCount * maximumNotes;
inline constexpr std::array<const char*, 8> chordNames {{
    "Major", "Minor", "Major 7", "Minor 7", "Sus 2", "Sus 4", "Fifth", "Octave"
}};
struct ChordDefinition
{
    std::array<int, maximumNotes> intervals;
    int count;
};
inline constexpr std::array<ChordDefinition, 8> chordDefinitions {{
    {{{0, 4, 7, 0}}, 3}, {{{0, 3, 7, 0}}, 3}, {{{0, 4, 7, 11}}, 4}, {{{0, 3, 7, 10}}, 4},
    {{{0, 2, 7, 0}}, 3}, {{{0, 5, 7, 0}}, 3}, {{{0, 7, 0, 0}}, 2}, {{{0, 12, 0, 0}}, 2}
}};

// Display helper only; the audio engine never constructs strings.
inline juce::String rootName(int midiNote)
{
    static constexpr std::array<const char*, 12> names {{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}};
    midiNote = std::clamp(midiNote, 0, 127);
    return juce::String(names[static_cast<size_t>(midiNote % 12)]) + juce::String(midiNote / 12 - 1);
}

inline int quantiseRoot(float root) noexcept
{
    return static_cast<int>(std::round(std::clamp(std::isfinite(root) ? root : static_cast<float>(defaultRoot),
                                                static_cast<float>(minimumRoot), static_cast<float>(maximumRoot))));
}
inline int quantiseChord(float chord) noexcept
{
    return static_cast<int>(std::round(std::clamp(std::isfinite(chord) ? chord : static_cast<float>(defaultChord),
                                                0.0f, static_cast<float>(chordDefinitions.size() - 1))));
}

struct Parameters
{
    float root = static_cast<float>(defaultRoot), chord = static_cast<float>(defaultChord);
    float color = 0.55f, decay = 0.6f, width = 0.7f;
};

// Input-excited modal bank. Pole rotation preserves state energy; the radius
// alone sets RT60. Excitation is approximately H2-normalised, and modal weights
// have unit squared sum, so increasing Decay does not mute broadband material.
class Engine
{
public:
    void prepare(double rate) noexcept
    {
        sampleRate = std::isfinite(rate) && rate >= 8000.0 && rate <= 768000.0 ? rate : 48000.0;
        controlPeriod = std::max(1, static_cast<int>(std::round(sampleRate * 0.001)));
        retuneLength = std::max(1, static_cast<int>(std::round(sampleRate * 0.025)));
        envelopeRelease = std::exp(-1.0 / (sampleRate * 0.1));
        protectionAttack = 1.0 - std::exp(-1.0 / (sampleRate * 0.0005));
        protectionRelease = 1.0 - std::exp(-1.0 / (sampleRate * 0.15));
        dcCoefficient = std::exp(-juce::MathConstants<double>::twoPi * 35.0 / sampleRate);
        dcInputScale = 0.5 * (1.0 + dcCoefficient);
        for (size_t note = 0; note < rotations.size(); ++note)
        {
            const auto fundamental = 440.0 * std::exp2((static_cast<double>(minimumRoot)
                + static_cast<double>(note) - 69.0) / 12.0);
            for (int harmonic = 0; harmonic < harmonicCount; ++harmonic)
            {
                const auto frequency = fundamental * (harmonic + 1);
                auto& rotation = rotations[note][static_cast<size_t>(harmonic)];
                rotation.valid = frequency < sampleRate * 0.45;
                const auto angle = juce::MathConstants<double>::twoPi * frequency / sampleRate;
                rotation.cosine = std::cos(angle);
                rotation.sine = std::sin(angle);
            }
        }
        reset();
    }

    void reset() noexcept
    {
        banks = {};
        activeBank = controlCounter = controlRamp = retuneRemaining = 0;
        initialised = false;
        inputEnvelope = wetEnvelope = lastDrivenEnvelope = 0.0;
        protectionGain = 1.0;
        dcPreviousInput.fill(0.0);
        dcPreviousOutput.fill(0.0);
    }

    void process(float& left, float& right, const Parameters& parameters, bool stereo = true) noexcept
    {
        processingStereo = stereo;
        const auto root = quantiseRoot(parameters.root), chord = quantiseChord(parameters.chord);
        const auto color = safe(parameters.color, 0.0f, 1.0f, 0.55f);
        const auto decay = safe(parameters.decay, 0.05f, 3.0f, 0.6f);
        const auto width = safe(parameters.width, 0.0f, 1.0f, 0.7f);
        if (! initialised)
        {
            updateControls(color, decay, width, true);
            buildBank(banks[0], root, chord);
            initialised = true;
        }
        if (controlCounter == 0)
        {
            updateControls(color, decay, width, false);
            controlCounter = controlPeriod;
        }
        --controlCounter;
        advanceControls();

        // A changing LFO never starts another retune in the middle of a fade.
        // At its end we accept only the latest requested root/chord, limiting
        // retunes to 40/s without passing through intermediate chord choices.
        if (retuneRemaining == 0 && (banks[static_cast<size_t>(activeBank)].root != root
            || banks[static_cast<size_t>(activeBank)].chord != chord))
        {
            buildBank(banks[static_cast<size_t>(1 - activeBank)], root, chord);
            retuneRemaining = retuneLength;
        }
        const std::array<double, 2> input {{std::isfinite(left) ? left : 0.0,
                                           stereo && std::isfinite(right) ? right : (std::isfinite(left) ? left : 0.0)}};
        auto output = renderBank(banks[static_cast<size_t>(activeBank)], input, stereo);
        if (retuneRemaining > 0)
        {
            const auto next = renderBank(banks[static_cast<size_t>(1 - activeBank)], input, stereo);
            const auto blend = static_cast<double>(retuneLength - retuneRemaining + 1) / retuneLength;
            for (size_t channel = 0; channel < output.size(); ++channel)
                output[channel] += blend * (next[channel] - output[channel]);
            if (--retuneRemaining == 0) activeBank = 1 - activeBank;
        }

        // A modal displacement response also passes static displacement.
        // Remove DC before it spends headroom in the resonance protector.
        // This pole's ~31 ms RT60 is shorter than the minimum 50 ms modal tail.
        for (size_t channel = 0; channel < output.size(); ++channel)
        {
            const auto filtered = dcCoefficient * dcPreviousOutput[channel]
                + dcInputScale * (output[channel] - dcPreviousInput[channel]);
            dcPreviousInput[channel] = output[channel];
            dcPreviousOutput[channel] = filtered;
            output[channel] = filtered;
        }

        const auto inputLevel = std::max(std::abs(input[0]), std::abs(input[1]));
        const auto wetLevel = std::max(std::abs(output[0]), std::abs(output[1]));
        inputEnvelope = std::max(inputLevel, inputEnvelope * envelopeRelease);
        wetEnvelope = std::max(wetLevel, wetEnvelope * envelopeRelease);
        const bool driven = inputLevel > 1.0e-12;
        if (driven) lastDrivenEnvelope = inputEnvelope;
        const bool changingGains = banks[static_cast<size_t>(activeBank)].gainRamp > 0
            || (retuneRemaining > 0 && banks[static_cast<size_t>(1 - activeBank)].gainRamp > 0);
        if (driven || changingGains)
        {
            const auto reference = driven ? inputEnvelope : lastDrivenEnvelope;
            const auto target = wetEnvelope > 1.0e-20 ? std::min(1.0, 2.0 * reference / wetEnvelope) : 1.0;
            const auto coefficient = target < protectionGain ? protectionAttack : protectionRelease;
            if (driven || target < protectionGain)
                protectionGain += coefficient * (target - protectionGain);
        }
        // Freeze gain in silence: a releasing detector must not lift the tail
        // and silently turn a specified RT60 into a much longer decay.
        left = protectedOutput(output[0] * protectionGain);
        right = stereo ? protectedOutput(output[1] * protectionGain) : left;
    }

private:
    struct Rotation { double cosine = 1.0, sine = 0.0; bool valid = false; };
    struct Mode
    {
        Rotation rotation;
        std::array<double, 2> real {}, imaginary {};
        // Left, right, mono. Mono never loses modes to stereo panning.
        std::array<double, 3> gain {}, targetGain {}, stepGain {};
    };
    struct Bank
    {
        std::array<Mode, maximumModes> modes;
        int root = defaultRoot, chord = defaultChord;
        int gainRamp = 0;
        bool ready = false;
    };
    static float safe(float value, float minimum, float maximum, float fallback) noexcept
    { return std::clamp(std::isfinite(value) ? value : fallback, minimum, maximum); }

    static float protectedOutput(double value) noexcept
    {
        if (! std::isfinite(value)) return 0.0f;
        const auto magnitude = std::abs(value);
        // Emergency soft headroom, outside ordinary nominal audio levels.
        // Pole stability and the envelope gain work independently of this.
        if (magnitude > 2.0)
            value = std::copysign(2.0 + 2.0 * std::tanh((magnitude - 2.0) * 0.5), value);
        return static_cast<float>(value);
    }

    void buildBank(Bank& bank, int root, int chord) noexcept
    {
        bank = {};
        bank.root = root; bank.chord = chord; bank.ready = true;
        const auto& definition = chordDefinitions[static_cast<size_t>(chord)];
        for (int note = 0; note < definition.count; ++note)
            for (int harmonic = 0; harmonic < harmonicCount; ++harmonic)
            {
                const auto tableNote = root + definition.intervals[static_cast<size_t>(note)] - minimumRoot;
                bank.modes[static_cast<size_t>(note * harmonicCount + harmonic)].rotation =
                    rotations[static_cast<size_t>(tableNote)][static_cast<size_t>(harmonic)];
            }
        setGains(bank, true);
    }

    void setGains(Bank& bank, bool snap) noexcept
    {
        if (! bank.ready) return;
        constexpr std::array<double, harmonicCount> brightness {{1.0, 0.85, 0.7, 0.58, 0.48, 0.4}};
        std::array<double, harmonicCount> harmonicWeight;
        harmonicWeight[0] = 1.0;
        for (size_t i = 1; i < harmonicWeight.size(); ++i) harmonicWeight[i] = lastColor * brightness[i];
        double energy = 0.0;
        for (size_t i = 0; i < bank.modes.size(); ++i)
            if (bank.modes[i].rotation.valid)
            {
                const auto weight = harmonicWeight[i % harmonicCount];
                energy += weight * weight;
            }
        const auto normalisation = energy > 0.0 ? 1.0 / std::sqrt(energy) : 0.0;
        const auto notes = chordDefinitions[static_cast<size_t>(bank.chord)].count;
        std::array<double, 2> change {};
        bool gainsChanged = false;
        for (size_t i = 0; i < bank.modes.size(); ++i)
        {
            auto& mode = bank.modes[i];
            const auto weight = mode.rotation.valid ? normalisation * harmonicWeight[i % harmonicCount] : 0.0;
            const auto position = 2.0 * static_cast<double>(i / harmonicCount) / (notes - 1) - 1.0;
            const auto pan = std::clamp(0.85 * lastWidth * position, -0.85, 0.85);
            mode.targetGain = {{weight * std::sqrt(1.0 - pan), weight * std::sqrt(1.0 + pan), weight}};
            for (size_t channel = 0; channel < (processingStereo ? size_t {2} : size_t {1}); ++channel)
            {
                const auto gainChannel = processingStereo ? channel : size_t {2};
                const auto difference = std::abs(mode.targetGain[gainChannel] - mode.gain[gainChannel]);
                gainsChanged = gainsChanged || difference > 0.0;
                change[channel] += difference * std::sqrt(mode.real[channel] * mode.real[channel]
                                                       + mode.imaginary[channel] * mode.imaginary[channel]);
            }
        }
        // Hidden modes can hold much more energy than their currently audible
        // gains reveal. Bound the speed of a requested gain change so the
        // existing envelope protection can follow it. Unselected harmonics do
        // not otherwise affect gain or attenuate the current sound.
        const auto amplitudeBudget = std::max(1.0e-8,
            2.0 * std::max(inputEnvelope, lastDrivenEnvelope) * protectionAttack * 0.5);
        const auto neededSamples = std::ceil(std::max(change[0], change[1]) * protectionGain / amplitudeBudget);
        bank.gainRamp = snap || ! gainsChanged ? 0 : static_cast<int>(std::clamp(neededSamples,
            static_cast<double>(controlPeriod), sampleRate * 0.1));
        for (auto& mode : bank.modes)
            for (size_t channel = 0; channel < mode.gain.size(); ++channel)
            {
                if (bank.gainRamp == 0) mode.gain[channel] = mode.targetGain[channel];
                mode.stepGain[channel] = bank.gainRamp == 0 ? 0.0
                    : (mode.targetGain[channel] - mode.gain[channel]) / bank.gainRamp;
            }
    }

    void updateControls(float color, float decay, float width, bool snap) noexcept
    {
        const bool decayChanged = snap || ! juce::exactlyEqual(decay, lastDecay);
        if (! snap && juce::exactlyEqual(color, lastColor) && ! decayChanged && juce::exactlyEqual(width, lastWidth)) return;
        lastColor = color; lastDecay = decay; lastWidth = width;
        // Matched pole radius: after Decay seconds amplitude is 10^(-60/20).
        if (decayChanged)
        {
            targetRadius = std::exp(-6.907755278982137 / (static_cast<double>(decay) * sampleRate));
            targetExcitation = std::sqrt(2.0 * (1.0 - targetRadius * targetRadius));
        }
        if (snap) { radius = targetRadius; excitation = targetExcitation; }
        radiusStep = (targetRadius - radius) / controlPeriod;
        excitationStep = (targetExcitation - excitation) / controlPeriod;
        for (auto& bank : banks) setGains(bank, snap);
        controlRamp = snap ? 0 : controlPeriod;
    }

    void advanceControls() noexcept
    {
        if (controlRamp > 0)
        {
            if (--controlRamp == 0) { radius = targetRadius; excitation = targetExcitation; }
            else { radius += radiusStep; excitation += excitationStep; }
        }
        for (auto& bank : banks)
            if (bank.ready && bank.gainRamp > 0)
            {
                const bool done = --bank.gainRamp == 0;
                for (auto& mode : bank.modes)
                    for (size_t channel = 0; channel < mode.gain.size(); ++channel)
                        mode.gain[channel] = done ? mode.targetGain[channel] : mode.gain[channel] + mode.stepGain[channel];
            }
    }

    std::array<double, 2> renderBank(Bank& bank, const std::array<double, 2>& input, bool stereo) noexcept
    {
        std::array<double, 2> output {};
        for (auto& mode : bank.modes)
        {
            if (! mode.rotation.valid) continue;
            for (size_t channel = 0; channel < (stereo ? size_t {2} : size_t {1}); ++channel)
            {
                const auto real = radius * (mode.rotation.cosine * mode.real[channel]
                                           - mode.rotation.sine * mode.imaginary[channel]) + excitation * input[channel];
                const auto imaginary = radius * (mode.rotation.sine * mode.real[channel]
                                                + mode.rotation.cosine * mode.imaginary[channel]);
                mode.real[channel] = real;
                mode.imaginary[channel] = imaginary;
                output[channel] += imaginary * mode.gain[stereo ? channel : size_t {2}];
            }
        }
        if (! stereo) output[1] = output[0];
        return output;
    }

    static constexpr size_t preparedNotes = static_cast<size_t>(maximumRoot + 12 - minimumRoot + 1);
    std::array<std::array<Rotation, harmonicCount>, preparedNotes> rotations {};
    std::array<Bank, 2> banks;
    double sampleRate = 48000.0, radius = 0.0, excitation = 0.0;
    double targetRadius = 0.0, targetExcitation = 0.0, radiusStep = 0.0, excitationStep = 0.0;
    double inputEnvelope = 0.0, wetEnvelope = 0.0, lastDrivenEnvelope = 0.0, protectionGain = 1.0;
    double envelopeRelease = 0.0, protectionAttack = 1.0, protectionRelease = 1.0;
    double dcCoefficient = 0.0, dcInputScale = 1.0;
    std::array<double, 2> dcPreviousInput {}, dcPreviousOutput {};
    float lastColor = -1.0f, lastDecay = -1.0f, lastWidth = -1.0f;
    int controlPeriod = 48, controlCounter = 0, controlRamp = 0;
    int retuneLength = 1200, retuneRemaining = 0, activeBank = 0;
    bool initialised = false, processingStereo = true;
};
} // namespace fire::chord_resonator

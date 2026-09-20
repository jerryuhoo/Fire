#pragma once

#include "../Utility/EqParameters.h"
#include <juce_dsp/juce_dsp.h>
#include <complex>

namespace fire::eq
{
struct Coefficients
{
    // Normalised b0,b1,b2,a0(=1),a1,a2. At most four cut sections and the
    // historical same-frequency gain/Q section; bell/shelf/notch use one.
    std::array<std::array<double, 6>, 5> stages {};
    int numStages = 0;

    double magnitudeAt(double frequency, double sampleRate) const noexcept
    {
        if (! std::isfinite(sampleRate) || sampleRate <= 0.0) return 1.0;
        const auto angle = -juce::MathConstants<double>::twoPi * frequency / sampleRate;
        const std::complex<double> z { std::cos(angle), std::sin(angle) };
        const auto z2 = z * z;
        double magnitude = 1.0;
        for (int stage = 0; stage < numStages; ++stage)
        {
            const auto& c = stages[static_cast<size_t>(stage)];
            const auto numerator = c[0] + c[1] * z + c[2] * z2;
            const auto denominator = 1.0 + c[4] * z + c[5] * z2;
            magnitude *= std::abs(numerator / denominator);
        }
        return std::isfinite(magnitude) ? magnitude : 1.0;
    }
};

inline Coefficients makeCoefficients(const NodeState& node, double sampleRate) noexcept
{
    Coefficients result;
    if (! node.present || node.bypassed) return result;
    sampleRate = std::isfinite(sampleRate) && sampleRate > 0.0 ? sampleRate : 48000.0;
    const auto maxFrequency = static_cast<double>(
        std::nextafter(static_cast<float>(sampleRate * 0.5), 0.0f));
    const auto frequency = juce::jlimit(juce::jmin(20.0, maxFrequency), maxFrequency,
                                      std::isfinite(node.frequency) ? static_cast<double>(node.frequency) : 1000.0);
    const auto q = juce::jlimit(0.1, 18.0, std::isfinite(node.q) ? static_cast<double>(node.q) : 0.70710678);
    const auto gain = juce::Decibels::decibelsToGain(juce::jlimit(-24.0, 24.0,
        std::isfinite(node.gainDb) ? static_cast<double>(node.gainDb) : 0.0));
    const auto append = [&](std::array<double, 6> values)
    {
        const auto a0 = values[3];
        for (auto& value : values) value /= a0;
        result.stages[static_cast<size_t>(result.numStages++)] = values;
    };
    using Array = juce::dsp::IIR::ArrayCoefficients<double>;
    switch (node.type)
    {
        case Type::lowCut:
        case Type::highCut:
        {
            const int count = juce::jlimit(1, 4, node.slope + 1);
            for (int stage = 0; stage < count; ++stage)
            {
                const auto angle = (2.0 * stage + 1.0) * juce::MathConstants<double>::pi / (4.0 * count);
                const auto butterworthQ = 1.0 / (2.0 * std::cos(angle));
                append(node.type == Type::lowCut ? Array::makeHighPass(sampleRate, frequency, butterworthQ)
                                                 : Array::makeLowPass(sampleRate, frequency, butterworthQ));
            }
            // Preserve Fire's original low/high-cut + resonance-EQ response.
            append(Array::makePeakFilter(sampleRate, frequency, q, gain));
            break;
        }
        case Type::lowShelf: append(Array::makeLowShelf(sampleRate, frequency, q, gain)); break;
        case Type::highShelf: append(Array::makeHighShelf(sampleRate, frequency, q, gain)); break;
        case Type::notch: append(Array::makeNotch(sampleRate, frequency, q)); break;
        case Type::bandPass: append(Array::makeBandPass(sampleRate, frequency, q)); break;
        case Type::bell: default: append(Array::makePeakFilter(sampleRate, frequency, q, gain)); break;
    }
    return result;
}
} // namespace fire::eq

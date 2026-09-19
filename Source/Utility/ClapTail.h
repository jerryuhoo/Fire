#pragma once

#include <cmath>
#include <cstdint>
#include <limits>

namespace fire::utility
{
// CLAP ext/tail.h reserves all sample counts >= INT32_MAX for infinity.
// JUCE's fast roundToInt cannot convert an infinite or overflowing double.
inline std::uint32_t tailSecondsToClapSamples(double seconds, double sampleRate) noexcept
{
    constexpr auto infinite = static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max());
    if (! std::isfinite(sampleRate) || sampleRate <= 0.0 || ! (seconds > 0.0)) return 0;
    if (std::isinf(seconds)) return infinite;
    const auto samples = seconds * sampleRate;
    if (! std::isfinite(samples) || samples >= static_cast<double>(infinite)) return infinite;
    return static_cast<std::uint32_t>(std::floor(samples + 0.5));
}
}

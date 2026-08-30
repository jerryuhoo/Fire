/*
  ==============================================================================

    StrictNumberParser.h
    Created: 30 Aug 2026

  ==============================================================================
*/

#pragma once

#include "juce_core/juce_core.h"

#include <cmath>

namespace fire::utility
{
/** Parses one complete finite decimal number using the locale-independent
    syntax used by JUCE state serialisation.

    Leading and trailing whitespace are accepted, but trailing characters,
    NaN, and infinities are rejected. The decimal separator is always '.'.
*/
inline bool parseStrictFiniteDouble(const juce::String& text,
                                    double& result) noexcept
{
    const auto trimmed = text.trim();
    if (trimmed.isEmpty())
        return false;

    auto cursor = trimmed.getCharPointer();
    const auto start = cursor;
    const auto parsed = juce::CharacterFunctions::readDoubleValue(cursor);
    if (cursor == start || ! cursor.isEmpty() || ! std::isfinite(parsed))
        return false;

    result = parsed;
    return true;
}

/** Parses a complete frequency value with an optional Hz, kHz, or k suffix.
    The numeric portion follows parseStrictFiniteDouble(), so malformed or
    trailing text is rejected rather than silently becoming zero.
*/
inline bool parseStrictFrequency(juce::String text,
                                 double& result) noexcept
{
    text = text.trim().toLowerCase();
    double multiplier = 1.0;

    if (text.endsWith("khz"))
    {
        multiplier = 1000.0;
        text = text.dropLastCharacters(3).trim();
    }
    else if (text.endsWithChar('k'))
    {
        multiplier = 1000.0;
        text = text.dropLastCharacters(1).trim();
    }
    else if (text.endsWith("hz"))
    {
        text = text.dropLastCharacters(2).trim();
    }

    double parsed = 0.0;
    if (! parseStrictFiniteDouble(text, parsed))
        return false;

    const auto frequency = parsed * multiplier;
    if (! std::isfinite(frequency))
        return false;

    result = frequency;
    return true;
}
} // namespace fire::utility

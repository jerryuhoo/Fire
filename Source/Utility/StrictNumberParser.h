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
} // namespace fire::utility

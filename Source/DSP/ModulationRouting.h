/*
  ==============================================================================

    ModulationRouting.h
    Created: 13 Sep 2025 7:56:13am
    Author:  Yifeng Yu

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include "../Utility/LfoBankParameters.h"
#include "../Utility/ModulationSources.h"
#include <cmath>

//==============================================================================
/**
    Describes a single connection from a source (LFO) to a target (Parameter).
    This struct is now in its own file to prevent circular dependencies.
*/
struct ModulationRouting
{
    int sourceLfoIndex = 0;
    juce::String targetParameterID = "";

    // Depth can be bipolar (-1.0 to 1.0), representing -100% to +100%
    float depth = 0.5f;
    bool isBipolar = true;
    bool isBypassed = false;

    // Endpoints are signed offsets in the destination's normalised range,
    // relative to its base knob value; depth scales both endpoints. Legacy
    // routes retain their exact Bi/Uni mapping until a custom range is edited.
    bool customRange = false;
    float rangeStart = 0.0f;
    float rangeEnd = 1.0f;
    int curve = 0; // Linear, Ease in, Ease out, S curve
    int depthMacro = -1; // -1 = fixed depth, otherwise Macro 1..4 at indices 0..3

    bool hasAdvancedMapping() const noexcept
    { return customRange || curve != 0 || depthMacro >= 0; }
    float minimumOffset() const noexcept { return customRange ? rangeStart : isBipolar ? -.5f : 0.0f; }
    float maximumOffset() const noexcept { return customRange ? rangeEnd : isBipolar ? .5f : 1.0f; }
    float offset(float source, float macro = 1.0f) const noexcept
    {
        auto value = juce::jlimit(0.0f, 1.0f, std::isfinite(source) ? source : 0.0f);
        if (curve == 1) value *= value;
        else if (curve == 2) value = 1.0f - (1.0f - value) * (1.0f - value);
        else if (curve == 3) value = value * value * (3.0f - 2.0f * value);
        const auto scale = depthMacro < 0 ? 1.0f : juce::jlimit(0.0f, 1.0f, std::isfinite(macro) ? macro : 0.0f);
        if (!customRange)
            return (isBipolar ? (value * 2.0f - 1.0f) * (depth * .5f) : value * depth) * scale;
        return (rangeStart + (rangeEnd - rangeStart) * value) * depth * scale;
    }
    bool hasSameState(const ModulationRouting& other) const noexcept
    {
        return sourceLfoIndex == other.sourceLfoIndex && targetParameterID == other.targetParameterID
            && depth == other.depth && isBipolar == other.isBipolar && isBypassed == other.isBypassed
            && customRange == other.customRange && rangeStart == other.rangeStart && rangeEnd == other.rangeEnd
            && curve == other.curve && depthMacro == other.depthMacro;
    }

    void sanitise() noexcept
    {
        sourceLfoIndex = juce::jlimit(0, fire::mod_sources::sourceCount - 1, sourceLfoIndex);
        depth = std::isfinite(depth) ? juce::jlimit(-1.0f, 1.0f, depth) : 0.5f;
        rangeStart = std::isfinite(rangeStart) ? juce::jlimit(-1.0f, 1.0f, rangeStart) : 0.0f;
        rangeEnd = std::isfinite(rangeEnd) ? juce::jlimit(-1.0f, 1.0f, rangeEnd) : 1.0f;
        curve = juce::jlimit(0, 3, curve);
        depthMacro = juce::jlimit(-1, fire::mod_sources::macroCount - 1, depthMacro);
    }

    // Helper for saving/loading state
    void writeToXml(juce::XmlElement& xml) const
    {
        xml.setAttribute("source", sourceLfoIndex);
        xml.setAttribute("target", targetParameterID);
        xml.setAttribute("depth", depth);
        xml.setAttribute("bipolar", isBipolar);
        xml.setAttribute("bypassed", isBypassed);
        xml.setAttribute("customRange", customRange);
        xml.setAttribute("rangeStart", rangeStart);
        xml.setAttribute("rangeEnd", rangeEnd);
        xml.setAttribute("curve", curve);
        xml.setAttribute("depthMacro", depthMacro);
    }

    static ModulationRouting readFromXml(const juce::XmlElement& xml)
    {
        ModulationRouting routing { xml.getIntAttribute("source", 0),
                                    xml.getStringAttribute("target"),
                                    static_cast<float>(xml.getDoubleAttribute("depth", 0.5)),
                                    xml.getBoolAttribute("bipolar", true),
                                    xml.getBoolAttribute("bypassed", false) };
        routing.customRange = xml.getBoolAttribute("customRange", false);
        routing.rangeStart = static_cast<float>(xml.getDoubleAttribute("rangeStart", 0.0));
        routing.rangeEnd = static_cast<float>(xml.getDoubleAttribute("rangeEnd", 1.0));
        routing.curve = xml.getIntAttribute("curve", 0);
        routing.depthMacro = xml.getIntAttribute("depthMacro", -1);
        routing.sanitise();
        return routing;
    }
};

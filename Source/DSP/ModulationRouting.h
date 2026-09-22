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

    void sanitise() noexcept
    {
        sourceLfoIndex = juce::jlimit(0, fire::lfo_bank::capacity - 1, sourceLfoIndex);
        depth = std::isfinite(depth) ? juce::jlimit(-1.0f, 1.0f, depth) : 0.5f;
    }

    // Helper for saving/loading state
    void writeToXml(juce::XmlElement& xml) const
    {
        xml.setAttribute("source", sourceLfoIndex);
        xml.setAttribute("target", targetParameterID);
        xml.setAttribute("depth", depth);
        xml.setAttribute("bipolar", isBipolar);
        xml.setAttribute("bypassed", isBypassed);
    }

    static ModulationRouting readFromXml(const juce::XmlElement& xml)
    {
        ModulationRouting routing { xml.getIntAttribute("source", 0),
                                    xml.getStringAttribute("target"),
                                    static_cast<float>(xml.getDoubleAttribute("depth", 0.5)),
                                    xml.getBoolAttribute("bipolar", true),
                                    xml.getBoolAttribute("bypassed", false) };
        routing.sanitise();
        return routing;
    }
};

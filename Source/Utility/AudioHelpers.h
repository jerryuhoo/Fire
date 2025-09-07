/*
  ==============================================================================

    AudioHelpers.h
    Created: 11 Apr 2021 1:07:30pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#pragma once
#include "juce_dsp/juce_dsp.h"

static inline float dBToNormalizedGain(float inValue)
{
    float inValuedB = juce::Decibels::gainToDecibels(inValue + 0.00001f);
    inValuedB = (inValuedB + 96.0f) / 96.0f;
    return inValuedB;
}

inline float helper_denormalize(float inValue)
{
    float absValue = fabs(inValue);
    if (absValue < 1e-15)
    {
        return 0.0f;
    }
    else
    {
        return inValue;
    }
}

// filter
enum Slope
{
    Slope_12,
    Slope_24,
    Slope_36,
    Slope_48
};

struct ChainSettings
{
    float peakFreq { 0 }, peakGainInDecibels { 0 }, peakQuality { 1.0f };
    float lowCutFreq { 0 }, highCutFreq { 0 }, lowCutQuality { 1.0f }, highCutQuality { 1.0f },
        lowCutGainInDecibels { 0 }, highCutGainInDecibels { 0 };

    Slope lowCutSlope { Slope::Slope_12 }, highCutSlope { Slope::Slope_12 };

    bool lowCutBypassed { false }, peakBypassed { false }, highCutBypassed { false };
};

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts);

enum ChainPositions
{
    LowCut,
    Peak,
    HighCut,
    LowCutQ,
    HighCutQ
};

using Filter = juce::dsp::IIR::Filter<float>;
using Coefficients = juce::dsp::IIR::Coefficients<float>;
using CoefficientsPtr = Filter::CoefficientsPtr;

using CutFilter = juce::dsp::ProcessorChain<Filter, Filter, Filter, Filter>;

using MonoChain = juce::dsp::ProcessorChain<CutFilter, Filter, CutFilter, Filter, Filter>;

CoefficientsPtr makePeakFilter(const ChainSettings& chainSettings, double sampleRate);
CoefficientsPtr makeLowcutQFilter(const ChainSettings& chainSettings, double sampleRate);
CoefficientsPtr makeHighcutQFilter(const ChainSettings& chainSettings, double sampleRate);

inline auto makeLowCutFilter(const ChainSettings& chainSettings, double sampleRate)
{
    return juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(chainSettings.lowCutFreq,
                                                                                       sampleRate,
                                                                                       2 * (chainSettings.lowCutSlope + 1));
}

inline auto makeHighCutFilter(const ChainSettings& chainSettings, double sampleRate)
{
    return juce::dsp::FilterDesign<float>::designIIRLowpassHighOrderButterworthMethod(chainSettings.highCutFreq,
                                                                                      sampleRate,
                                                                                      2 * (chainSettings.highCutSlope + 1));
}

void updateCoefficients(CoefficientsPtr& old, const CoefficientsPtr& replacements);

template <int Index, typename ChainType, typename CoefficientType>

void update(ChainType& chain, const CoefficientType& coefficients)
{
    updateCoefficients(chain.template get<Index>().coefficients, coefficients[Index]);
    chain.template setBypassed<Index>(false);
}

template <typename ChainType, typename CoefficientType>
void updateCutFilter(ChainType& chain,
                     const CoefficientType& coefficients,
                     const Slope& slope)
{
    chain.template setBypassed<0>(true);
    chain.template setBypassed<1>(true);
    chain.template setBypassed<2>(true);
    chain.template setBypassed<3>(true);

    switch (slope)
    {
        case Slope_48:
        {
            update<3>(chain, coefficients);
        }
        case Slope_36:
        {
            update<2>(chain, coefficients);
        }
        case Slope_24:
        {
            update<1>(chain, coefficients);
        }
        case Slope_12:
        {
            update<0>(chain, coefficients);
        }
    }
}

namespace ParameterIDAndName
{
    // A struct to hold all necessary info for a modulatable parameter.
    // Defining it inside the namespace keeps our code nicely organized.
    struct ModulatableParameterInfo
    {
        juce::String name; // The UI-facing name, e.g., "Comp Ratio"
        juce::String idBase; // The processor-facing ID base, e.g., "compRatio"
    };

    struct ModulationTarget
    {
        juce::String displayText; // e.g., "Drive (Band 1)"
        juce::String parameterID; // e.g., "drive1"
    };

    // Define the parameter version number in one central place.
    const int versionNum = 1;

    // --- Part 1: Functions that return juce::String ---

    /**
     * @brief [String] Creates the raw string ID for an indexed parameter.
     * @param base      The base part of the parameter ID (e.g., DRIVE_ID).
     * @param index     The zero-based index of the band or channel.
     * @return juce::String Returns the final string ID (e.g., "drive1").
     */
    static inline juce::String getIDString(const juce::String& base, int index)
    {
        return base + juce::String(index + 1);
    }

    /**
     * @brief [String] Gets the raw string ID for a global (non-indexed) parameter.
     * @param base      The parameter ID (e.g., HQ_ID).
     * @return juce::String Returns the base string itself.
     */
    static inline juce::String getIDString(const juce::String& base)
    {
        return base;
    }

    /**
     * @brief [String] Creates the display name for an indexed parameter.
     * @param base      The base part of the parameter name (e.g., "Drive").
     * @param index     The zero-based index of the band or channel.
     * @return juce::String Returns the final display name (e.g., "Drive 1").
     */
    static inline juce::String getName(const juce::String& base, int index)
    {
        return base + " " + juce::String(index + 1);
    }

    // --- Part 2: Functions that return juce::ParameterID ---

    /**
     * @brief [ParameterID] Creates a full juce::ParameterID object for an indexed parameter.
     * @param base      The base part of the parameter ID (e.g., DRIVE_ID).
     * @param index     The zero-based index of the band or channel.
     * @return juce::ParameterID Returns the complete ParameterID object (e.g., {"drive1", 1}).
     */
    static inline juce::ParameterID getID(const juce::String& base, int index)
    {
        return { getIDString(base, index), versionNum };
    }

    /**
     * @brief [ParameterID] Creates a full juce::ParameterID object for a global parameter.
     * @param base      The parameter ID (e.g., HQ_ID).
     * @return juce::ParameterID Returns the complete ParameterID object (e.g., {"hq", 1}).
     */
    static inline juce::ParameterID getID(const juce::String& base)
    {
        return { base, versionNum };
    }

    /** * @brief Returns a definitive list of parameter names that are eligible for modulation.
     *
     * This static function provides a single source of truth for all parts of the plugin
     * that need to know which parameters can be targeted by an LFO. This includes:
     * - The BandPanel, for creating the ModulatableSlider components.
     * - The PluginEditor, for updating the modulation arcs on the sliders.
     * - The ModulationMatrixPanel, for populating the target selection dropdown menu.
     *
     * @return A constant reference to a vector of juce::String objects.
     */
    static const std::vector<juce::String>& getModulatableParameterNames()
    {
        // This static vector is initialized only once, the first time this function is called.
        static const std::vector<juce::String> modulatableNames = {
            DRIVE_NAME,
            COMP_RATIO_NAME,
            COMP_THRESH_NAME,
            WIDTH_NAME,
            OUTPUT_NAME,
            MIX_NAME,
            BIAS_NAME,
            REC_NAME
        };
        return modulatableNames;
    }

    /** * @brief Returns a definitive list of all modulatable parameters,
     * including their UI Name and their ID base.
     * This is the single source of truth for the entire application.
     */
    static const std::vector<ModulatableParameterInfo>& getModulatableParameterInfo()
    {
        // This static vector is initialized only once.
        static const std::vector<ModulatableParameterInfo> parameters = {
            { DRIVE_NAME, DRIVE_ID },
            { COMP_RATIO_NAME, COMP_RATIO_ID },
            { COMP_THRESH_NAME, COMP_THRESH_ID },
            { WIDTH_NAME, WIDTH_ID },
            { OUTPUT_NAME, OUTPUT_ID },
            { MIX_NAME, MIX_ID },
            { BIAS_NAME, BIAS_ID },
            { REC_NAME, REC_ID }
        };
        return parameters;
    }

    /**
     * @brief Returns a list of all modulation targets.
     *
     * This function generates a list of all possible modulation targets
     * by combining each modulatable parameter with each band.
     *
     * @return A vector of ModulationTarget objects.
     */
    static std::vector<ModulationTarget> getAllModulatableTargets()
    {
        std::vector<ModulationTarget> targets;
        const auto& modulatableParams = getModulatableParameterInfo();

        for (int i = 0; i < 4; ++i)
        {
            for (const auto& paramInfo : modulatableParams)
            {
                targets.push_back({ paramInfo.name + " (Band " + juce::String(i + 1) + ")",
                                    getIDString(paramInfo.idBase, i) });
            }
        }
        return targets;
    }
} // namespace ParameterIDAndName
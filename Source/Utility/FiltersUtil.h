/*
  ==============================================================================

    FiltersUtil.h
    Created: 15 Sep 2025 12:13:49am
    Author:  Yifeng Yu

  ==============================================================================
*/

#pragma once

#include "../Utility/Parameters.h" // For Slope enum
#include "juce_audio_processors/juce_audio_processors.h"
#include "juce_dsp/juce_dsp.h"

// Forward declare LfoManager to avoid including the full header
class LfoManager;

enum ChainPositions
{
    LowCut,
    Peak,
    HighCut,
    LowCutQ,
    HighCutQ
};

// Data structure to hold all settings for a filter chain.
struct ChainSettings
{
    float peakFreq { 0 }, peakGainInDecibels { 0 }, peakQuality { 1.0f };
    float lowCutFreq { 0 }, highCutFreq { 0 }, lowCutQuality { 1.0f }, highCutQuality { 1.0f },
        lowCutGainInDecibels { 0 }, highCutGainInDecibels { 0 };

    Slope lowCutSlope { Slope::Slope_12 }, highCutSlope { Slope::Slope_12 };

    bool lowCutBypassed { false }, peakBypassed { false }, highCutBypassed { false };
};

// Functions to get settings from the APVTS
ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts);
ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts, LfoManager& lfoManager);

// Type aliases for clarity
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
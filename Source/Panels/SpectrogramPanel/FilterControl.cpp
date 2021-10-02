/*
  ==============================================================================

    FilterControl.cpp
    Created: 18 Sep 2021 9:54:04pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include <JuceHeader.h>
#include "FilterControl.h"
#include "../../GUI/InterfaceDefines.h"

//==============================================================================
FilterControl::FilterControl(FireAudioProcessor &p) : processor(p)
{
    const auto& params = processor.getParameters();
    for( auto param : params )
    {
        param->addListener(this);
    }

    updateChain();
    startTimerHz(60);
}

FilterControl::~FilterControl()
{
    const auto& params = processor.getParameters();
    for( auto param : params )
    {
        param->removeListener(this);
    }
}

void FilterControl::paint (juce::Graphics& g)
{
    g.setColour(juce::Colours::hotpink.withBrightness(0.8f));
    g.strokePath(responseCurve, juce::PathStrokeType(2.f));
    g.setColour(juce::Colours::hotpink.withBrightness(0.8f).withAlpha(0.2f));
    g.fillPath(responseCurve);
}

void FilterControl::resized()
{
    updateResponseCurve();
}

void FilterControl::setParams(float lowCut,
                              
                              float highCut,
                              float cutRes,
                              float peak,
                              float peakRes)
{
    mLowCut = lowCut;
    mHighCut = highCut;
    mCutRes = cutRes;
    mPeak = peak;
    mPeakRes = peakRes;
}

void FilterControl::updateChain()
{
    auto chainSettings = getChainSettings(processor.treeState);
    
    monoChain.setBypassed<ChainPositions::LowCut>(chainSettings.lowCutBypassed);
    monoChain.setBypassed<ChainPositions::Peak>(chainSettings.peakBypassed);
    monoChain.setBypassed<ChainPositions::HighCut>(chainSettings.highCutBypassed);
    
    auto peakCoefficients = makePeakFilter(chainSettings, processor.getSampleRate());
    updateCoefficients(monoChain.get<ChainPositions::Peak>().coefficients, peakCoefficients);
    
    auto lowCutCoefficients = makeLowCutFilter(chainSettings, processor.getSampleRate());
    auto highCutCoefficients = makeHighCutFilter(chainSettings, processor.getSampleRate());
    
    updateCutFilter(monoChain.get<ChainPositions::LowCut>(),
                    lowCutCoefficients,
                    chainSettings.lowCutSlope);
    
    updateCutFilter(monoChain.get<ChainPositions::HighCut>(),
                    highCutCoefficients,
                    chainSettings.highCutSlope);
}

void FilterControl::updateResponseCurve()
{
    int w = getWidth();
    
    auto& lowcut = monoChain.get<ChainPositions::LowCut>();
    auto& peak = monoChain.get<ChainPositions::Peak>();
    auto& highcut = monoChain.get<ChainPositions::HighCut>();
    
    auto sampleRate = processor.getSampleRate();
    
    std::vector<float> mags;
    mags.resize(w);
    
    for(int i = 0; i < w; ++i)
    {
        double mag = 1.0f;
        auto freq = juce::mapToLog10(double(i) / double(w), 20.0, 20000.0);

        if( !monoChain.isBypassed<ChainPositions::Peak>() )
            mag *= peak.coefficients->getMagnitudeForFrequency(freq, sampleRate);
        
        if( !monoChain.isBypassed<ChainPositions::LowCut>() )
        {
            if( !lowcut.isBypassed<0>() )
                mag *= lowcut.get<0>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if( !lowcut.isBypassed<1>() )
                mag *= lowcut.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if( !lowcut.isBypassed<2>() )
                mag *= lowcut.get<2>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if( !lowcut.isBypassed<3>() )
                mag *= lowcut.get<3>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        }
        
        if(!monoChain.isBypassed<ChainPositions::HighCut>())
        {
            if( !highcut.isBypassed<0>() )
                mag *= highcut.get<0>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if( !highcut.isBypassed<1>() )
                mag *= highcut.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if( !highcut.isBypassed<2>() )
                mag *= highcut.get<2>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if( !highcut.isBypassed<3>() )
                mag *= highcut.get<3>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        }
        
        mags[i] = juce::Decibels::gainToDecibels(mag);
    }
    
    responseCurve.clear();
        
    const double outputMin = getHeight();
    const double outputMax = 0;
    auto map = [outputMin, outputMax](double input)
    {
        return juce::jmap(input, -24.0, 24.0, outputMin, outputMax);
    };
    
    juce::Point<float> startPoint(-3, getHeight() + 2);
    juce::Point<float> endPoint(getWidth() + 3, getHeight() + 2);
    
    responseCurve.startNewSubPath(startPoint);
    
    for( size_t i = 0; i < mags.size(); ++i )
    {
        responseCurve.lineTo(0 + i, map(mags[i]));
    }
    
    responseCurve.lineTo(endPoint);
    
    responseCurve.closeSubPath();
}

void FilterControl::parameterValueChanged(int parameterIndex, float newValue)
{
    parametersChanged.set(true);
}

void FilterControl::timerCallback()
{
    if( parametersChanged.compareAndSetBool(false, true) )
    {
        updateChain();
        updateResponseCurve();
    }
    
    repaint();
}

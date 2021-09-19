/*
  ==============================================================================

    FilterControl.cpp
    Created: 18 Sep 2021 9:54:04pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include <JuceHeader.h>
#include "FilterControl.h"
#include "../GUI/InterfaceDefines.h"
//==============================================================================
FilterControl::FilterControl()
{
  
}

FilterControl::~FilterControl()
{
}

void FilterControl::paint (juce::Graphics& g)
{
    int w = getWidth();
    
    std::vector<float> mags;
    mags.resize(w);
    
    for( int i = 0; i < w; ++i )
    {
        double mag = 1.0f;
        auto freq = juce::mapToLog10(double(i) / double(w), 20.0, 20000.0);
        /*
         
        if(! monoChain.isBypassed<ChainPositions::Peak>() )
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
        
        if( !monoChain.isBypassed<ChainPositions::HighCut>() )
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
         */
        mags[i] = juce::Decibels::gainToDecibels(mag);
    }
    
    responseCurve.clear();
        
    const double outputMin = getHeight();
    const double outputMax = 0;
    auto map = [outputMin, outputMax](double input)
    {
        return juce::jmap(input, -24.0, 24.0, outputMin, outputMax);
    };
    
    responseCurve.startNewSubPath(0, map(mags.front()));
    
    for( size_t i = 1; i < mags.size(); ++i )
    {
        responseCurve.lineTo(0 + i, map(mags[i]));
    }
    
    g.setColour(KNOB_SUBFONT_COLOUR);
    g.strokePath(responseCurve, juce::PathStrokeType(2.f));
}

void FilterControl::resized()
{
   
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

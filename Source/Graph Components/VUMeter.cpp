/*
  ==============================================================================

    VUMeter.cpp
    Created: 25 Jan 2021 2:55:04pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include <JuceHeader.h>
#include "VUMeter.h"
#include "../GUI/InterfaceDefines.h"

//==============================================================================
VUMeter::VUMeter(FireAudioProcessor* inProcessor)
:   mParameterId(-1),
    mCh0Level(0),
    mCh1Level(0),
    mProcessor(inProcessor)
{
    // In your constructor, you should add any child components, and
    // initialise any special settings that your component needs.

}

VUMeter::~VUMeter()
{
}

void VUMeter::paint (juce::Graphics& g)
{
    g.setColour(COLOUR6);
    const int meterWidth = getWidth() / 3;
    
    // left
    g.fillRect(0, 0, meterWidth, getHeight());
    // right
    g.fillRect(meterWidth * 2, 0, meterWidth, getHeight());
    
    int ch0fill = getHeight() - (getHeight() * mCh0Level);
    int ch1fill = getHeight() - (getHeight() * mCh1Level);
    
    if (ch0fill < 0)
    {
        ch0fill = 0;
    }
    if (ch1fill < 0)
    {
        ch1fill = 0;
    }
    
    g.setColour(COLOUR5);
    g.fillRect(0, ch0fill, meterWidth, getHeight());
    g.fillRect(meterWidth * 2, ch1fill, meterWidth, getHeight());
}

void VUMeter::resized()
{
}

void VUMeter::setParameterId(int inParameterId)
{
    mParameterId = inParameterId;
    startTimerHz(30);
}

void VUMeter::timerCallback()
{
    float updatedCh0Level = 0.0f;
    float updatedCh1Level = 0.0f;
//    switch (mParameterId) {
//        case <#constant#>:
//            <#statements#>
//            break;
//            
//        default:
//            break;
//    }
}

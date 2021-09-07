/*
  ==============================================================================

    VUPanel.cpp
    Created: 29 Aug 2021 6:21:02pm
    Author:  羽翼深蓝Wings

  ==============================================================================
*/

#include <JuceHeader.h>
#include "VUPanel.h"

//==============================================================================
VUPanel::VUPanel(FireAudioProcessor &p) : processor(p), vuMeterIn(&p), vuMeterOut(&p)
{
    // In your constructor, you should add any child components, and
    // initialise any special settings that your component needs.
    
    vuMeterIn.setParameterId(1);
    
    vuMeterOut.setParameterId(2);
    
    addAndMakeVisible(vuMeterIn);
    addAndMakeVisible(vuMeterOut);
}

VUPanel::~VUPanel()
{
}

void VUPanel::paint (juce::Graphics& g)
{
    g.setColour(COLOUR6);
    g.drawRect (getLocalBounds(), 1);   // draw an outline around the component
}

void VUPanel::resized()
{
    // This method is where you should set the bounds of any child
    // components that your component contains..
    vuMeterIn.setBounds(VU_METER_X_1, VU_METER_Y, VU_METER_WIDTH, VU_METER_HEIGHT);
    vuMeterOut.setBounds(VU_METER_X_2, VU_METER_Y, VU_METER_WIDTH, VU_METER_HEIGHT);
}

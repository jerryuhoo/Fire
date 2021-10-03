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
    
    // draw compressor threshold line
    g.setColour(KNOB_SUBFONT_COLOUR);
    
    juce::String threshID = "";
    
    if (focusBandNum == 0) threshID = COMP_THRESH_ID1;
    else if (focusBandNum == 1) threshID = COMP_THRESH_ID2;
    else if (focusBandNum == 2) threshID = COMP_THRESH_ID3;
    else if (focusBandNum == 3) threshID = COMP_THRESH_ID4;
    else jassertfalse;
    
    float threshValue = *(processor.treeState.getRawParameterValue(threshID));
    float compressorLineY = VU_METER_Y + VU_METER_HEIGHT * -threshValue / 96.0f; // 96 is VU meter range
    g.drawLine(VU_METER_X_1, compressorLineY, VU_METER_X_1 - getWidth() / 20, compressorLineY - getHeight() / 20, 1);
    g.drawLine(VU_METER_X_1 - getWidth() / 20, compressorLineY - getHeight() / 20, VU_METER_X_1 - getWidth() / 20, compressorLineY + getHeight() / 20, 1);
    g.drawLine(VU_METER_X_1, compressorLineY, VU_METER_X_1 - getWidth() / 20, compressorLineY + getHeight() / 20, 1);
    
    if (mZoomState)
    {
        float textX = (VU_METER_X_1 + VU_METER_X_2 - VU_METER_WIDTH) / 2.0f;
        float text20Y = VU_METER_Y + 20.0f / 96.0f * VU_METER_HEIGHT;
        float text40Y = VU_METER_Y + 40.0f / 96.0f * VU_METER_HEIGHT;
        float text60Y = VU_METER_Y + 60.0f / 96.0f * VU_METER_HEIGHT;
        float text80Y = VU_METER_Y + 80.0f / 96.0f * VU_METER_HEIGHT;
        g.drawText("  0", textX, VU_METER_Y, getWidth() / 5, getHeight() / 10, juce::Justification::centred);
        g.drawText("-20", textX, text20Y, getWidth() / 5, getHeight() / 10, juce::Justification::centred);
        g.drawText("-40", textX, text40Y, getWidth() / 5, getHeight() / 10, juce::Justification::centred);
        g.drawText("-60", textX, text60Y, getWidth() / 5, getHeight() / 10, juce::Justification::centred);
        g.drawText("-80", textX, text80Y, getWidth() / 5, getHeight() / 10, juce::Justification::centred);
    }
}

void VUPanel::resized()
{
    // This method is where you should set the bounds of any child
    // components that your component contains..
    vuMeterIn.setBounds(VU_METER_X_1, VU_METER_Y, VU_METER_WIDTH, VU_METER_HEIGHT);
    vuMeterOut.setBounds(VU_METER_X_2, VU_METER_Y, VU_METER_WIDTH, VU_METER_HEIGHT);
}

bool VUPanel::getZoomState()
{
    return mZoomState;
}

void VUPanel::setZoomState(bool zoomState)
{
    mZoomState = zoomState;
}

void VUPanel::mouseDown(const juce::MouseEvent &e)
{
    if (mZoomState)
    {
        mZoomState = false;
    }
    else
    {
        mZoomState = true;
    }
}

void VUPanel::setFocusBandNum(int num)
{
    focusBandNum = num;
}
